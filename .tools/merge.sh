#!/usr/bin/env bash
# 주차별 원본 레포를 [clone -> git-filter-repo로 대용량 제거 -> git subtree 병합]하고,
# _slides/NN 의 발표자료를 NN/docs 로 옮깁니다. 원본 레포는 절대 변경하지 않습니다.
# 사용법:  bash .tools/merge.sh            # repos.txt 전체
#          bash .tools/merge.sh 03 07      # 특정 주차만
set -uo pipefail

# ---- 히스토리에서 제거할 것들 (필요하면 수정) ----
MAX_BLOB="50M"                 # 이 크기 초과 blob 제거
STRIP_DIRS=(Binaries Intermediate DerivedDataCache Saved Library Temp Logs .vs obj)

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
LIST=".tools/repos.txt"
TMP="${MERGE_TMP:-$ROOT/.tools/tmp}"
ONLY=("$@")

want() {
  [ ${#ONLY[@]} -eq 0 ] && return 0
  local w; for w in "${ONLY[@]}"; do [ "$w" = "$1" ] && return 0; done
  return 1
}

place_slides() {
  local wk="$1"
  [ -d "_slides/$wk" ] || return 0
  local n; n=$(ls -A "_slides/$wk"); [ -n "$n" ] || return 0
  mkdir -p "$wk/docs"
  mv "_slides/$wk"/* "$wk/docs"/
  rmdir "_slides/$wk" 2>/dev/null
  git add "$wk/docs"
  git commit -q -m "docs($wk): 발표자료 추가" && echo "    slides -> $wk/docs"
}

filter_args() {  # git-filter-repo 인자 조립
  local d
  echo "--strip-blobs-bigger-than $MAX_BLOB --invert-paths"
  for d in "${STRIP_DIRS[@]}"; do
    echo "--path $d/ --path-glob */$d/*"
  done
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

  if [ -z "${branch:-}" ]; then
    branch=$(git ls-remote --symref "$url" HEAD 2>/dev/null \
             | sed -n 's#^ref: refs/heads/\([^\t]*\)\tHEAD#\1#p')
    [ -z "$branch" ] && branch=main
  fi

  work="$TMP/$wk"
  rm -rf "$work"; mkdir -p "$TMP"

  echo "[$wk] clone  $url ($branch)"
  if ! git clone --quiet --no-local --branch "$branch" --single-branch "$url" "$work"; then
    echo "  !! [$wk] clone 실패"; fail=1; continue
  fi

  echo "[$wk] filter (blob>$MAX_BLOB, 빌드 산출물 제거)"
  if ! git -C "$work" filter-repo --quiet $(filter_args | tr '\n' ' ') --force; then
    echo "  !! [$wk] filter-repo 실패"; fail=1; rm -rf "$work"; continue
  fi

  echo "[$wk] subtree add"
  rmdir "$wk" 2>/dev/null
  if git subtree add --prefix="$wk" "$work" "$branch" \
       -m "merge($wk): $url ($branch) 병합 (blob>$MAX_BLOB 및 빌드 산출물 제거)"; then
    place_slides "$wk"
  else
    echo "  !! [$wk] subtree 실패"; fail=1
  fi
  rm -rf "$work"
done < "$LIST"

rmdir "$TMP" 2>/dev/null
echo
echo "완료. (실패 있음: $fail)"
git log --oneline | head -20
