# init_db.py
import sys
from typing import Optional
from database import init as dbcore

RETENTION_DAYS = 30

def _ask_yesno(msg: str, default_no: bool = True) -> bool:
    ans = input(f"{msg} [{'y' if not default_no else 'Y'}/{'N' if default_no else 'n'}] ").strip().lower()
    if ans == "y": return True
    if ans == "n" or ans == "": return not True if not default_no else False
    return False

def init_or_reset_interactive() -> None:
    drop = _ask_yesno(">> 빈 DB로 초기화할까요? (모든 테이블 삭제)", default_no=True)
    dbcore.init_db(drop_all=drop)
    print("✅ 완료")

def clean_old_records() -> int:
    deleted = dbcore.clean_old_records("sensor_data", RETENTION_DAYS)
    print(f"✅ {RETENTION_DAYS}일 이전 sensor_data 삭제: {deleted}건")
    return deleted

def _usage():
    print("사용법:\n"
          "  python init_db.py           # 대화형 초기화(존재 시 물어봄)\n"
          "  python init_db.py init      # 대화형 초기화\n"
          "  python init_db.py clean     # 30일 이전 sensor_data 정리\n")

if __name__ == "__main__":
    if len(sys.argv) == 1:
        init_or_reset_interactive()
        sys.exit(0)
    cmd = sys.argv[1].lower()
    if cmd == "init":
        init_or_reset_interactive()
    elif cmd == "clean":
        clean_old_records()
    else:
        _usage()
