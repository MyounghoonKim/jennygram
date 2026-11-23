#!/bin/bash
# 제니그램 워크플로의 실행이 (스스로 이어 도는 회차까지) 하나도 남지 않을 때까지 기다린다.
# 취소된 중복 실행이 마지막에 있어도 진행 중인 실행이 있으면 계속 기다린다.
# 사용: bash tools/jennygram/wait-runs.sh <워크플로 파일 이름> [최대 대기 시간(시간 단위, 기본 26)]
# 마지막 줄에 "끝: <워크플로> <마지막으로 끝난 실행 번호> <결론>"을 찍는다.
REPO="MyounghoonKim/jennygram"
WF="${1:?워크플로 파일 이름이 필요합니다}"
MAX_HOURS="${2:-26}"
LOG="${HOME}/.local/state/mkay/jennygram/wait-runs-${WF%.yml}.log"
DEADLINE=$(( $(date +%s) + MAX_HOURS * 3600 ))
mkdir -p "$(dirname "$LOG")"
idle=0
while [ "$(date +%s)" -lt "$DEADLINE" ]; do
  rows=$(gh run list --repo "$REPO" --workflow "$WF" --limit 10 \
    --json databaseId,status,conclusion --jq '.[] | [.databaseId, .status, (.conclusion // "")] | @tsv' 2>/dev/null)
  if [ -z "$rows" ]; then
    echo "$(date '+%y. %-m. %-d. %H:%M:%S') 조회 실패" >> "$LOG"
    sleep 60
    continue
  fi
  active=$(printf '%s\n' "$rows" | awk -F'\t' '$2 != "completed"' | wc -l | tr -d ' ')
  echo "$(date '+%y. %-m. %-d. %H:%M:%S') 진행 중 ${active}개" >> "$LOG"
  if [ "$active" -eq 0 ]; then
    idle=$((idle + 1))
  else
    idle=0
  fi
  # 진행 중인 실행이 3분 동안 없으면 이어 도는 회차도 없는 것으로 본다.
  if [ "$idle" -ge 3 ]; then
    lastdone=$(printf '%s\n' "$rows" | awk -F'\t' '$3 != "cancelled"' | head -1)
    echo "끝: ${WF} $(printf '%s' "$lastdone" | cut -f1) $(printf '%s' "$lastdone" | cut -f3)"
    exit 0
  fi
  sleep 60
done
echo "끝: ${WF} 시간 초과 ${MAX_HOURS}시간"
exit 1
