#!/usr/bin/env python3
"""Preview or apply a roster workshop to the Tessera source catalog and artwork."""
import argparse
from pathlib import Path
import sys

from workshop_import import ROOT, Plan, apply_plan, pending_transactions, restore_import


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('draft', nargs='?', type=Path, help='Workshop/group JSON; defaults to the LAN shared save')
    parser.add_argument('--root', type=Path, default=ROOT, help='Tessera checkout (default: this checkout)')
    parser.add_argument('--group', action='append', default=[], help='Group name or ID; repeat to select several (default: all nonempty groups)')
    parser.add_argument('--art-source', action='append', default=[], type=Path, help='Search folder for new artwork; recursive, repeatable')
    parser.add_argument('--art-map', type=Path, help='JSON mapping card IDs/final names to image paths')
    parser.add_argument('--accept-unconfirmed', action='store_true', help='Accept assigned designs without individual workshop confirmation flags')
    parser.add_argument('--allow-incomplete', action='store_true', help='Import assigned slots in partially filled groups')
    parser.add_argument('--placeholder-missing-art', action='store_true', help='Use marked Tessera card-back placeholders for missing art; ambiguous/invalid images still require resolution')
    parser.add_argument('--report-dir', type=Path, help='Preview output folder (default: .tessera-workshop/import-preview)')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--apply', action='store_true', help='Apply the validated plan with backups; does not build or install the mod')
    mode.add_argument('--restore', type=Path, help='Restore a prior or interrupted import, if its outputs have not changed')
    args = parser.parse_args()
    root = args.root.expanduser().resolve()
    try:
        if args.restore:
            if args.draft or args.group or args.art_source or args.art_map or args.accept_unconfirmed or args.allow_incomplete or args.placeholder_missing_art:
                parser.error('--restore cannot be combined with import options')
            restore_import(root, args.restore)
            print(f'Restored import: {args.restore}')
            return 0
        pending_transactions(root)
        plan = Plan(root, args.draft or root / '.tessera-workshop/workshop.json', groups=args.group,
                    art_sources=args.art_source, art_map=args.art_map,
                    accept_unconfirmed=args.accept_unconfirmed, allow_incomplete=args.allow_incomplete,
                    placeholder_missing_art=args.placeholder_missing_art)
        report_dir = args.report_dir or root / '.tessera-workshop/import-preview'
        plan.write_report(report_dir)
        s = plan.report['summary']
        print(f"{s['selected']} assigned: {s['updated']} existing cards changed, {s['new']} new; {s['artWrites']} artwork writes.")
        print(f"Preserving {plan.report['preserved']} other existing identities. Proposed catalog: {s['catalogSize']} cards.")
        if s['placeholders']:
            print(f"{s['placeholders']} cards use card-back placeholders.")
        print(f'Report: {report_dir.resolve() / "report.md"}')
        if plan.errors:
            print(f'{len(plan.errors)} import checks need attention; no catalog or artwork changes made.')
            return 2
        if args.apply:
            backup = apply_plan(plan)
            print(f'Applied. Backup: {backup}' if backup else 'Already applied; no files changed.')
            print('Source catalog and artwork only. Review game integration findings before building/installing.')
        else:
            print('Preview only; no catalog or artwork changes made. Add --apply to import this selection.')
        return 0
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(f'Workshop import stopped: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
