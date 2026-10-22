#!/usr/bin/env python3
"""Exercise shared saves, concurrency, recovery, and the LAN server's file boundary."""
import copy
from concurrent.futures import ThreadPoolExecutor
import http.client
import importlib.util
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch
import uuid

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("workshop_server", ROOT / "tools/serve-roster-workshop.py")
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


def blank(data):
    groups = []
    for t in data["templates"]:
        groups.append({"kind": "tessera-catalog-template", "schema": 2,
                       "catalogFingerprint": data["fingerprint"], "sideOrder": data["policy"]["side_order"],
                       "template": {k: t[k] for k in ("id", "revision", "group", "label")},
                       "slots": [dict(copy.deepcopy(s), assignment=None, confirmed=False) for s in t["slots"]]})
    return {"kind": "tessera-roster-workshop", "schema": 2, "catalogFingerprint": data["fingerprint"],
            "sideOrder": data["policy"]["side_order"], "currentGroup": "dark-elf-v2", "groups": groups}


class SharedWorkshopTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = server.Catalog()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.store = server.SharedStore(self.temp.name, self.catalog, backups=3)
        self.http = server.WorkshopServer(("127.0.0.1", 0), self.catalog, self.store)
        self.thread = threading.Thread(target=self.http.serve_forever, daemon=True)
        self.thread.start()
        self.doc = blank(self.catalog.data)

    def tearDown(self):
        self.http.shutdown()
        self.http.server_close()
        self.thread.join()
        self.store.close()
        self.temp.cleanup()

    def request(self, method="GET", path="/api/workshop", doc=None, revision="empty", headers=None):
        client = http.client.HTTPConnection("127.0.0.1", self.http.server_port, timeout=10)
        defaults = {"Content-Type": "application/json", "X-Tessera-Workshop": "1",
                    "X-Workshop-Change": uuid.uuid4().hex, "If-Match": '"' + revision + '"'}
        defaults.update(headers or {})
        body = json.dumps(doc) if doc is not None else None
        client.request(method, path, body, defaults)
        response = client.getresponse()
        result = response.status, dict(response.getheaders()), response.read()
        client.close()
        return result

    def test_save_restart_backups_and_importable_files(self):
        status, _, raw = self.request()
        self.assertEqual(status, 200)
        self.assertIsNone(json.loads(raw)["workshop"])
        revision = "empty"
        for i in range(6):
            self.doc["groups"][0]["slots"][0]["affinities"] = ["Strength"] if i % 2 else ["Agility"]
            status, _, raw = self.request("PUT", doc=self.doc, revision=revision)
            self.assertEqual(status, 200, raw)
            revision = json.loads(raw)["revision"]
        backups = list(self.store.backup_dir.glob("*.json"))
        self.assertEqual(len(backups), 3)
        for path in backups:
            self.catalog.validate(json.loads(path.read_text()))
        self.assertEqual(self.store.read()["workshop"], self.doc)
        self.store.close()
        self.store = server.SharedStore(self.temp.name, self.catalog, backups=3)
        self.http.store = self.store
        self.assertEqual(self.store.read()["workshop"], self.doc)
        self.assertEqual(self.store.read()["revision"], revision)
        status, headers, _ = self.request(headers={"If-None-Match": '"' + revision + '"'})
        self.assertEqual(status, 304)
        self.assertEqual(headers["ETag"], '"' + revision + '"')
        with self.assertRaises(BlockingIOError):
            server.SharedStore(self.temp.name, self.catalog)

    def test_simultaneous_saves_and_idempotent_retry(self):
        change = uuid.uuid4().hex
        with ThreadPoolExecutor(max_workers=2) as pool:
            replies = list(pool.map(lambda _: self.request("PUT", doc=self.doc), range(2)))
        self.assertEqual(sorted(r[0] for r in replies), [200, 409])
        revision = self.store.read()["revision"]
        self.doc["groups"][0]["slots"].pop()
        first = self.request("PUT", doc=self.doc, revision=revision, headers={"X-Workshop-Change": change})
        second = self.request("PUT", doc=self.doc, revision=revision, headers={"X-Workshop-Change": change})
        self.assertEqual(first, second)
        self.assertEqual(len(list(self.store.backup_dir.glob("*.json"))), 1)
        self.assertEqual(self.request("PUT", doc=self.doc, revision="empty")[0], 409)

    def test_bad_drafts_and_write_failures_preserve_previous_save(self):
        self.request("PUT", doc=self.doc)
        original = self.store.path.read_bytes()
        revision = self.store.read()["revision"]
        for invalid in (None, [], {"kind": "wrong"}):
            self.assertEqual(self.request("PUT", doc=invalid, revision=revision)[0], 400 if invalid is not None else 413)
        altered = copy.deepcopy(self.doc)
        altered["groups"][1]["slots"][0]["sides"][0] = True
        self.assertEqual(self.request("PUT", doc=altered, revision=revision)[0], 400)
        altered = copy.deepcopy(self.doc)
        for group in altered["groups"][:2]:
            group["slots"][0]["assignment"] = {"cardId": self.catalog.data["cards"][0]["id"], "name": "Repeated art"}
        self.assertEqual(self.request("PUT", doc=altered, revision=revision)[0], 400)
        altered = copy.deepcopy(self.doc)
        altered["groups"][0]["slots"][0]["confirmed"] = True
        self.assertEqual(self.request("PUT", doc=altered, revision=revision)[0], 400)
        with patch.object(server.os, "replace", side_effect=OSError("disk full")):
            self.assertEqual(self.request("PUT", doc=self.doc, revision=revision)[0], 503)
        self.assertEqual(self.store.path.read_bytes(), original)
        self.assertEqual(self.store.read()["revision"], revision)

    def test_serving_and_request_boundaries(self):
        status, _, raw = self.request(path="/docs/catalog-template-builder.html")
        self.assertEqual(status, 200)
        self.assertIn(b'content="/api/workshop"', raw)
        route = next(k for k in self.catalog.assets if k.endswith(".webp"))
        status, headers, raw = self.request(path=route.replace(" ", "%20"))
        self.assertEqual(status, 200)
        self.assertEqual(headers["Content-Type"], "image/webp")
        self.assertGreater(len(raw), 100)
        self.assertEqual(self.request(path="/TesseraUI/art/backs/mosaic.webp")[0], 200)
        for path in ("/docs/card-library.html", "/docs/card-library.csv", "/docs/expansion-editor.html"):
            status, _, raw = self.request(path=path)
            self.assertEqual(status, 200)
            self.assertEqual(raw, (ROOT / path.lstrip("/")).read_bytes())
        _, headers, _ = self.request(path="/docs/expansion-editor.html")
        self.assertIn("img-src 'self' data: blob:", headers["Content-Security-Policy"])
        for path in ("/.git/config", "/.tessera-workshop/workshop.json", "/api/../data/cards.json", "/docs/", "/%2e%2e/etc/passwd"):
            self.assertEqual(self.request(path=path)[0], 404)
        for headers in ({"Origin": "http://evil.example"}, {"Host": "evil.example:8765"},
                        {"X-Tessera-Workshop": ""}, {"Sec-Fetch-Site": "cross-site"}):
            self.assertEqual(self.request("PUT", doc=self.doc, headers=headers)[0], 403)
        self.assertEqual(self.request("PUT", doc=self.doc, headers={"If-Match": ""})[0], 428)
        self.assertEqual(self.request("PUT", doc=self.doc, headers={"Content-Length": str(server.MAX_BODY + 1)})[0], 413)
        self.assertEqual(self.store.read()["revision"], "empty")

    def test_new_card_identities(self):
        a, b = self.doc["groups"][0]["slots"][:2]
        a["assignment"] = {"kind": "new", "cardId": "new-" + uuid.uuid4().hex, "name": "Hist Dreamer"}
        b["assignment"] = {"kind": "new", "cardId": "new-" + uuid.uuid4().hex, "name": "Root Warden"}
        a["confirmed"] = True
        self.assertEqual(self.request("PUT", doc=self.doc)[0], 200)
        saved = self.store.read()
        self.assertEqual(saved["workshop"]["groups"][0]["slots"][0], a)
        invalid = copy.deepcopy(self.doc)
        invalid["groups"][0]["slots"][1]["assignment"]["cardId"] = a["assignment"]["cardId"]
        self.assertEqual(self.request("PUT", doc=invalid, revision=saved["revision"])[0], 400)
        for identity in ("missing", self.catalog.data["cards"][0]["id"], "new-not-a-uuid"):
            invalid["groups"][0]["slots"][1]["assignment"]["cardId"] = identity
            self.assertEqual(self.request("PUT", doc=invalid, revision=saved["revision"])[0], 400)
        del invalid["groups"][0]["slots"][0]["assignment"]["kind"]
        self.assertEqual(self.request("PUT", doc=invalid, revision=saved["revision"])[0], 400)
        self.assertEqual(self.store.read()["revision"], saved["revision"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
