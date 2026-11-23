#!/bin/bash
# 제니그램 윈도우 빌드가 (스스로 이어 도는 회차까지) 모두 끝날 때까지 기다린다.
# 사용: bash tools/jennygram/wait-build.sh [최대 대기 시간(시간 단위, 기본 26)]
# 마지막 줄에 "끝: <실행 번호> <결론>"을 찍고 끝난다. 로그는 ~/.local/state/mkay/jennygram/wait-build-mac.log
REPO="MyounghoonKim/jennygram"
LOG="${HOME}/.local/state/mkay/jennygram/wait-build-mac.log"
MAX_HOURS="${1:-26}"
DEADLINE=$(( $(date +%s) + MAX_HOURS * 3600 ))
mkdir -p "$(dirname "$LOG")"
stable=0
last=""
while [ "$(date +%s)" -lt "$DEADLINE" ]; do
  line=$(gh run list --repo "$REPO" --workflow jennygram-mac.yml --limit 1 \
    --json databaseId,status,conclusion --jq '.[] | [.databaseId, .status, (.conclusion // "")] | @tsv' 2>/dev/null)
  echo "$(date '+%y. %-m. %-d. %H:%M:%S') ${line:-조회 실패}" >> "$LOG"
  status=$(printf '%s' "$line" | cut -f2)
  if [ "$status" = "completed" ] && [ "$line" = "$last" ]; then
    stable=$((stable + 1))
  else
    stable=0
  fi
  last="$line"
  # 끝난 실행이 3분 동안 그대로 마지막이면 이어 도는 회차가 없는 것으로 본다.
  if [ "$stable" -ge 3 ]; then
    echo "맥 끝: $(printf '%s' "$line" | cut -f1) $(printf '%s' "$line" | cut -f3)"
    exit 0
  fi
  sleep 60
done
echo "맥 끝: 시간 초과 ${MAX_HOURS}시간"
exit 1
