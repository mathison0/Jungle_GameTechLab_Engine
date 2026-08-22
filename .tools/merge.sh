#!/usr/bin/env bash
# 주차별 원본 레포를 git subtree로 병합하고, _slides/NN 의 발표자료를 NN/docs 로 옮깁니다.
# 사용법:  bash .tools/merge.sh            # repos.txt 전체
#          bash .tools/merge.sh 03 07      # 특정 주차만
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
LIST=".tools/repos.txt"
ONLY=("$@")

want() {  # 인자로 주차를 지정했으면 그것만 처리
  [ ${#ONLY[@]} -eq 0 ] && return 0
  local w; for w in "${ONLY[@]}"; do [ "$w" = "$1" ] && return 0; done
  return 1
}

place_slides() {  # $1 = 주차
  local wk="$1"
  [ -d "_slides/$wk" ] || return 0
  local n; n=$(ls -A "_slides/$wk"); [ -n "$n" ] || return 0
  mkdir -p "$wk/docs"
  mv "_slides/$wk"/* "$wk/docs"/
  rmdir "_slides/$wk" 2>/dev/null
  git add "$wk/docs"
  git commit -q -m "docs($wk): 발표자료 추가" && echo "    slides -> $wk/docs"
}

fail=0
while read -r wk url branch; do
  case "${wk:-}" in ''|'#'*) continue;; esac
  want "$wk" || continue

  if [ -z "${url:-}" ]; then
    echo "[$wk] URL 미지정 — 발표자료만 배치"
    mkdir -p "$wk"; place_slides "$wk"
    continue
  fi

  if [ -d "$wk" ] && [ -n "$(git log --oneline -1 -- "$wk" 2>/dev/null)" ]; then
    echo "[$wk] 이미 병합됨 — 건너뜀"
    continue
  fi

  # 기본 브랜치 자동 탐지
  if [ -z "${branch:-}" ]; then
    branch=$(git ls-remote --symref "$url" HEAD 2>/dev/null \
             | sed -n 's#^ref: refs/heads/\([^\t]*\)\tHEAD#\1#p')
    [ -z "$branch" ] && branch=main
  fi

  echo "[$wk] subtree add  $url  ($branch)"
  rmdir "$wk" 2>/dev/null   # subtree add 는 대상 폴더가 없어야 함
  if git subtree add --prefix="$wk" "$url" "$branch" -m "merge($wk): $url ($branch) 병합"; then
    place_slides "$wk"
  else
    echo "  !! [$wk] 실패 — URL/브랜치/접근권한 확인 필요"
    fail=1
  fi
done < "$LIST"

echo
echo "완료. (실패 있음: $fail)"
git log --oneline | head -20
