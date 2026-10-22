#!/usr/bin/env python3
"""Serve the roster workshop on a trusted LAN, with shared saves and backups."""
import argparse
import base64
import copy
from datetime import datetime, timezone
import fcntl
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ipaddress
import json
import mimetypes
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import threading
import time
from urllib.parse import unquote, urljoin, urlsplit
import uuid

ROOT = Path(__file__).resolve().parents[1]
MAX_BODY = 8 * 1024 * 1024
MARKER = '<meta name="workshop-save-api" content="">'


def timestamp():
    return datetime.now(timezone.utc).isoformat(timespec="microseconds")


class InvalidDraft(ValueError):
    pass


class Conflict(Exception):
    pass


class Catalog:
    def __init__(self, root=ROOT):
        self.root = Path(root).resolve()
        page = (self.root / "docs/catalog-template-builder.html").read_text()
        if MARKER not in page:
            raise ValueError("Rebuild the workshop: python3 tools/build-catalog-audit.py")
        self.page = page.replace(MARKER, MARKER.replace('content=""', 'content="/api/workshop"')).encode()
        self.data = json.loads(re.search(r'<script type="application/json" id="builder-data">(.*?)</script>', page, re.S)[1])
        self.templates = {t["id"]: t for t in self.data["templates"]}
        self.cards = {c["id"] for c in self.data["cards"]}
        self.affinities = set(self.data["workshopPolicy"]["affinities"])
        self.assets = {}
        for art in [c[key] for c in self.data["cards"] for key in ("art", "thumbnail")] + [self.data["cardBack"]]:
            route = unquote(urljoin("/docs/catalog-template-builder.html", art))
            path = (self.root / route.lstrip("/")).resolve()
            if not path.is_relative_to(self.root / "TesseraUI/art"):
                raise ValueError("Artwork is outside the catalog art directory")
            self.assets[route] = path
        # Explicit files only: never expose repository browsing or saved drafts.
        for filename in ("catalog-audit.html", "card-library.html", "card-library.csv", "expansion-editor.html",
                         "reference-guide.md", "card-art-replacers.md"):
            self.assets["/docs/" + filename] = self.root / "docs" / filename

    def validate(self, doc):
        """Accept incomplete editable drafts, but enforce identity and schema integrity.

        Confirmed slots must also satisfy the embedded rank policy. Use the exact
        bitmaps shipped with this workshop, rather than a potentially newer policy.
        """
        def require(ok, text):
            if not ok:
                raise InvalidDraft(text)

        try:
            require(isinstance(doc, dict) and doc.get("kind") == "tessera-roster-workshop" and
                    type(doc.get("schema")) is int and doc["schema"] == 2, "Choose a workshop export.")
            require(doc["catalogFingerprint"] == self.data["fingerprint"], "This draft belongs to a different catalog.")
            require(doc["sideOrder"] == self.data["policy"]["side_order"], "Unexpected side order.")
            require(doc["currentGroup"] in self.templates, "Unknown group.")
            require(isinstance(doc["groups"], list) and len(doc["groups"]) == len(self.templates), "Missing groups.")
            seen_groups, seen_cards, seen_slots, names, confirmed = set(), set(), set(), {}, []
            for group in doc["groups"]:
                require(group["kind"] == "tessera-catalog-template" and type(group["schema"]) is int and group["schema"] == 2,
                        "Invalid group format.")
                require(group["catalogFingerprint"] == doc["catalogFingerprint"] and group["sideOrder"] == doc["sideOrder"],
                        "Inconsistent catalog or side order.")
                t = self.templates[group["template"]["id"]]
                require(t["id"] not in seen_groups and group["template"]["revision"] == t["revision"] and
                        group["template"]["group"] == t["group"], "Repeated group or changed template revision.")
                seen_groups.add(t["id"])
                require(isinstance(group["slots"], list) and len(group["slots"]) <= 500, "Invalid slot list.")
                original = {s["id"]: s for s in t["slots"]}
                presets = {s["id"]: s for s in t["presets"]}
                for s in group["slots"]:
                    sid, tier = s["id"], s["tier"]
                    require(type(tier) is int and tier in t["tiers"] and isinstance(sid, str) and
                            re.fullmatch(r"[a-z0-9-]{1,100}", sid) and sid not in seen_slots, "Invalid or repeated slot.")
                    require((sid in original and original[sid]["tier"] == tier) or
                            (sid not in original and sid.startswith(f'{t["slug"]}-{tier}-added-')),
                            "Unknown or moved slot.")
                    require(presets[s["preset"]]["tier"] == tier, "Invalid prepared profile.")
                    seen_slots.add(sid)
                    require(s["rarity"] in self.data["rarities"] and type(s["unique"]) is bool and
                            type(s["confirmed"]) is bool, "Invalid rarity or confirmation.")
                    require(isinstance(s["sides"], list) and len(s["sides"]) == 4 and
                            all(type(n) is int and 1 <= n <= 10 for n in s["sides"]), "Invalid side values.")
                    require(isinstance(s["affinities"], list) and len(s["affinities"]) <= 5 and
                            all(a in self.affinities for a in s["affinities"]) and
                            len(set(s["affinities"])) == len(s["affinities"]), "Invalid affinities.")
                    a = s["assignment"]
                    if a is not None:
                        is_new = a.get("kind") == "new"
                        known = (isinstance(a["cardId"], str) and re.fullmatch(r"new-[0-9a-f]{32}", a["cardId"])) if is_new else ("kind" not in a and a["cardId"] in self.cards)
                        require(known and a["cardId"] not in seen_cards, "Unknown or repeated card.")
                        require(isinstance(a["name"], str) and len(a["name"]) <= 120 and
                                not re.search(r"[\x00-\x1f\x7f]", a["name"]), "Invalid card name.")
                        seen_cards.add(a["cardId"])
                        name = a["name"].strip().lower()
                        names[name] = names.get(name, 0) + 1
                    if s["confirmed"]:
                        require(a is not None and a["name"].strip(), "An empty slot cannot be confirmed.")
                        confirmed.append(a["name"].strip().lower())
                        require(s["rarity"] in t["allowedRarities"][str(tier)] and
                                s["unique"] == (s["rarity"] == "Legendary"), "Invalid confirmed rarity.")
                        bits = base64.b64decode(self.data["rankChecks"][str(tier)]["Common" if s["rarity"] == "Common" else "Other"])
                        index = 0
                        for n in s["sides"]:
                            index = index * 10 + n - 1
                        require(bits[index >> 3] & (1 << (index % 8)), "Invalid confirmed side values.")
            require(all(names[n] == 1 for n in confirmed), "A confirmed card has a duplicate name.")
        except (KeyError, TypeError, IndexError, AttributeError) as exc:
            raise InvalidDraft("Invalid workshop structure.") from exc
        result = copy.deepcopy(doc)
        result.pop("sharedSave", None)
        return result


def atomic_json(path, doc):
    """A failed write cannot truncate the previous save."""
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent, prefix=".writing-", delete=False) as stream:
            temporary = Path(stream.name)
            json.dump(doc, stream, ensure_ascii=False, indent=2, allow_nan=False)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        fd = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    finally:
        if temporary and temporary.exists():
            temporary.unlink()


class SharedStore:
    def __init__(self, directory, catalog, backups=100):
        self.directory, self.catalog, self.backups = Path(directory).resolve(), catalog, backups
        self.directory.mkdir(parents=True, exist_ok=True)
        self.path = self.directory / "workshop.json"
        self.backup_dir = self.directory / "backups"
        self.backup_dir.mkdir(exist_ok=True)
        self.lock = threading.Lock()
        self.process_lock = (self.directory / "server.lock").open("a")
        try:
            fcntl.flock(self.process_lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.record = json.loads(self.path.read_text()) if self.path.exists() else None
            if self.record is not None:
                catalog.validate(self.record)
                meta = self.record["sharedSave"]
                if not re.fullmatch(r"[0-9a-f]{32}", meta["revision"]) or not isinstance(meta["savedAt"], str):
                    raise ValueError("Invalid saved revision")
        except Exception:
            self.close()
            raise

    def close(self):
        self.process_lock.close()

    def read(self):
        with self.lock:
            doc = copy.deepcopy(self.record)
        if doc is None:
            return {"revision": "empty", "savedAt": None, "workshop": None}
        meta = doc.pop("sharedSave")
        return {"revision": meta["revision"], "savedAt": meta["savedAt"], "workshop": doc}

    def write(self, doc, expected, change_id):
        doc = self.catalog.validate(doc)
        with self.lock:
            previous = self.record
            meta = previous["sharedSave"] if previous else {"revision": "empty"}
            # Retrying a request after a lost response is safe and creates no backup.
            if meta.get("changeId") == change_id and meta.get("parentRevision") == expected:
                if {k: v for k, v in previous.items() if k != "sharedSave"} == doc:
                    return {"revision": meta["revision"], "savedAt": meta["savedAt"]}
            if meta["revision"] != expected:
                raise Conflict("Another session saved newer changes. Load the shared version before saving.")
            saved = timestamp()
            doc["sharedSave"] = {"revision": uuid.uuid4().hex, "savedAt": saved,
                                 "parentRevision": expected, "changeId": change_id}
            if previous:
                name = meta["savedAt"].replace(":", "-") + "-" + meta["revision"] + ".json"
                atomic_json(self.backup_dir / name, previous)
            atomic_json(self.path, doc)
            self.record = doc
            # Pruning is best effort; an already committed save must still be acknowledged.
            for path in sorted(self.backup_dir.glob("*.json"))[:-self.backups]:
                try:
                    path.unlink()
                except OSError:
                    pass
            return {"revision": doc["sharedSave"]["revision"], "savedAt": saved}


class WorkshopServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, catalog, store):
        self.catalog, self.store = catalog, store
        self.hostnames = {"localhost", socket.gethostname().lower(), socket.getfqdn().lower()}
        super().__init__(address, WorkshopHandler)


class WorkshopHandler(BaseHTTPRequestHandler):
    server_version = "TesseraWorkshop/1"

    def setup(self):
        super().setup()
        self.connection.settimeout(15)

    def allowed(self):
        """LAN/loopback clients, literal IP or this host's name; no cross-origin writes."""
        try:
            peer = ipaddress.ip_address(self.client_address[0])
            host = urlsplit("http://" + self.headers.get("Host", ""))
            host_ok = host.hostname in self.server.hostnames
            if not host_ok:
                addr = ipaddress.ip_address(host.hostname)
                host_ok = addr.is_private or addr.is_loopback
            return (peer.is_private or peer.is_loopback) and host_ok and host.port == self.server.server_port
        except (ValueError, TypeError):
            return False

    def reply(self, status, payload, kind="application/json; charset=utf-8", etag=None):
        body = json.dumps(payload, ensure_ascii=False).encode() if not isinstance(payload, bytes) else payload
        self.send_response(status)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store" if not kind.startswith("image/") else "public, max-age=3600")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("X-Frame-Options", "DENY")
        self.send_header("Referrer-Policy", "same-origin")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'")
        if etag:
            self.send_header("ETag", '"' + etag + '"')
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        if not self.allowed():
            return self.reply(403, {"error": "Use this computer's LAN address or localhost."})
        route = unquote(urlsplit(self.path).path)
        if route == "/":
            self.send_response(302)
            self.send_header("Location", "/docs/catalog-template-builder.html")
            self.send_header("Content-Length", "0")
            self.end_headers()
        elif route == "/api/workshop":
            state = self.server.store.read()
            if self.headers.get("If-None-Match") == '"' + state["revision"] + '"':
                return self.reply(304, b"", etag=state["revision"])
            self.reply(200, state, etag=state["revision"])
        elif route == "/docs/catalog-template-builder.html":
            self.reply(200, self.server.catalog.page, "text/html; charset=utf-8")
        elif route in self.server.catalog.assets:
            path = self.server.catalog.assets[route]
            try:
                if path.is_symlink() or not path.resolve().is_relative_to(self.server.catalog.root):
                    return self.reply(404, {"error": "Not found."})
                self.reply(200, path.read_bytes(), mimetypes.guess_type(path)[0] or "application/octet-stream")
            except OSError:
                self.reply(404, {"error": "Not found."})
        else:
            self.reply(404, {"error": "Not found."})

    def do_PUT(self):
        if (not self.allowed() or self.headers.get("X-Tessera-Workshop") != "1" or
                self.headers.get("Origin", "http://" + self.headers.get("Host", "")) != "http://" + self.headers.get("Host", "") or
                self.headers.get("Sec-Fetch-Site") == "cross-site"):
            return self.reply(403, {"error": "Save from the workshop page on this server."})
        if urlsplit(self.path).path != "/api/workshop":
            return self.reply(404, {"error": "Not found."})
        expected = self.headers.get("If-Match", "")
        change_id = self.headers.get("X-Workshop-Change", "")
        if not re.fullmatch(r'"(?:empty|[0-9a-f]{32})"', expected):
            return self.reply(428, {"error": "A save revision is required."})
        if not re.fullmatch(r"[a-z0-9-]{1,100}", change_id):
            return self.reply(400, {"error": "A change identifier is required."})
        if self.headers.get("Content-Type", "").split(";")[0] != "application/json" or self.headers.get("Transfer-Encoding"):
            return self.reply(415, {"error": "Send JSON with Content-Length."})
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= MAX_BODY:
                return self.reply(413, {"error": "Workshop save is too large or empty."})
            raw = self.rfile.read(length)
            if len(raw) != length:
                return self.reply(400, {"error": "Incomplete request."})
            doc = json.loads(raw, parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Invalid JSON number")))
            saved = self.server.store.write(doc, expected[1:-1], change_id)
            self.reply(200, saved, etag=saved["revision"])
        except Conflict as exc:
            self.reply(409, {"error": str(exc)})
        except (ValueError, UnicodeError, RecursionError) as exc:
            self.reply(400, {"error": str(exc) if isinstance(exc, InvalidDraft) else "Invalid JSON save."})
        except OSError:
            self.reply(503, {"error": "The host could not write the save. Your previous save is intact."})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--state-dir", type=Path, default=ROOT / ".tessera-workshop")
    parser.add_argument("--backups", type=int, default=100)
    parser.add_argument("--background", action="store_true", help="Detach and write server.log/server.pid in the state directory")
    args = parser.parse_args()
    if args.backups < 1 or not 0 <= args.port <= 65535:
        parser.error("Use at least one backup and a valid port.")
    if args.background:
        directory = args.state_dir.resolve()
        directory.mkdir(parents=True, exist_ok=True)
        log = directory / "server.log"
        with log.open("a") as stream:
            child = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "--bind", args.bind,
                                      "--port", str(args.port), "--state-dir", str(directory),
                                      "--backups", str(args.backups)], stdin=subprocess.DEVNULL,
                                     stdout=stream, stderr=stream, start_new_session=True, close_fds=True)
        for _ in range(100):
            if child.poll() is not None:
                parser.exit(1, f"Server could not start. See {log}\n")
            pid_file = directory / "server.pid"
            if pid_file.exists() and pid_file.read_text().strip() == str(child.pid):
                print(f"Workshop server running (PID {child.pid}). Log: {log}")
                return
            time.sleep(0.1)
        child.terminate()
        child.wait()
        parser.exit(1, f"Server did not become ready. See {log}\n")
    store = None
    try:
        catalog = Catalog()
        store = SharedStore(args.state_dir, catalog, args.backups)
        with WorkshopServer((args.bind, args.port), catalog, store) as server:
            print(f"Tessera workshop listening on {args.bind}:{server.server_port}", flush=True)
            print(f"Open http://localhost:{server.server_port}/ on this computer, or its LAN address remotely.", flush=True)
            print(f"Shared save: {store.path}\nBackups: {store.backup_dir} (last {args.backups})", flush=True)
            (store.directory / "server.pid").write_text(str(os.getpid()) + "\n")
            server.serve_forever()
    except KeyboardInterrupt:
        pass
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f"Workshop server could not start: {exc}\nExisting saves were not changed.\n")
    finally:
        if store:
            store.close()


if __name__ == "__main__":
    main()
