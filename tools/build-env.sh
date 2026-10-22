#!/usr/bin/env bash
# Shared values for optional local build commands.
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct)}"
