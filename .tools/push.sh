#!/usr/bin/env bash
# 5GB 레포를 GitHub 팩 한도(2GB/푸시) 아래로 나눠서 푸시합니다.
# first-parent 마일스톤(주차 병합 커밋)을 순서대로 밀어 올립니다.
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

for c in $(git log --reverse --first-parent --format=%H main); do
  echo ">>> push $(git log -1 --format='%h %s' "$c")"
  if ! git push origin "$c":refs/heads/main; then
    echo "!! 푸시 실패: $c — 여기서 중단"; exit 1
  fi
done
git push -u origin main
echo "푸시 완료"
