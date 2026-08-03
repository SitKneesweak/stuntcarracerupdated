#!/bin/bash
# Commit everything and push to origin.
# Double-click in Finder, or run: ./push.command "commit message"
set -e
cd "$(dirname "$0")"

# Keep the Terminal window open long enough to read the result.
trap 'echo; printf "Press return to close..."; read -r _' EXIT

BRANCH=$(git rev-parse --abbrev-ref HEAD)

if [ -z "$(git status --porcelain)" ]; then
	echo "Nothing to commit."
else
	git add -A
	git status --short
	MSG="$*"
	if [ -z "$MSG" ]; then
		printf "Commit message: "
		read -r MSG
	fi
	[ -z "$MSG" ] && { echo "No message, aborting."; exit 1; }
	git commit -m "$MSG"
fi

echo "Pushing $BRANCH to origin..."
git push -u origin "$BRANCH"
echo "Done."
