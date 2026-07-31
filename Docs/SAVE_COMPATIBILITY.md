# 세이브 호환성 출시 계약

문서 버전: `Save-v1`

대상 소비자: 현재 출시 후보의 `IndieGame Win64 Shipping`

지원 주장 범위: 외부 스키마 v1, v2, v3

현재 코드는 외부 `UIGSaveGame::CurrentSchemaVersion=3`과 내부 REBIRTH
스냅샷 스키마 3을 사용한다. 이 문서는 v1·v2·v3 세이브를 실제 배포
환경에서 검증하는 픽스처, 오라클과 증거 규약을 고정한다.

문서나 마이그레이션 코드가 존재한다는 사실은 호환성 증거가 아니다.
현재 저장소에는 출처 태그가 고정된 v1·v2·v3 `.sav` 픽스처, 픽스처
매니페스트와 clean Shipping 실행 결과가 없다. 특히 이력에는 v1 소스
커밋이 있지만 불변 태그가 없고, v2 전용 생산 태그와 바이너리도 확인되지
않는다. 따라서 현재 G6 세이브 호환성 판정은 **BLOCKED**다.

## 변경 불가 원칙

1. 각 `.sav`는 해당 스키마를 실제로 기록한 **과거 불변 태그의 원본
   Shipping 빌드**에서 생성한다.
2. 현재 v3 클래스의 메모리 레이아웃으로 v1·v2를 직렬화하거나, 현재
   빌드의 디버그 명령으로 구버전 번호만 덮어쓰거나, hex editor로 바이너리를
   수정한 파일은 픽스처가 아니다.
3. 생산 태그에는 픽스처를 만드는 입력 절차나 fixture-only 생산 코드도
   함께 있어야 한다. 오라클은 그 태그의 공개 필드와 실제 게임 진행으로
   작성하고, 소비자 코드의 정규화 결과를 보고 사후 수정하지 않는다.
4. 픽스처 원본은 읽기 전용으로 보관한다. 테스트마다 복사본을 사용하고
   실행 전후 원본 SHA-256이 같은지 검사한다.
5. 호환성은 에디터·Development가 아니라 Unreal Editor, Visual Studio와
   개발용 DLL이 없는 clean Windows VM 스냅샷 또는 별도 물리 PC에서
   Shipping 패키지로 승인한다.
6. 픽스처, 오라클, Shipping 패키지, 실행 로그와 결과 JSON은 모두
   40자리 소스 SHA와 파일별 SHA-256으로 연결한다.

v2를 실제로 저장한 과거 소스와 태그를 확보할 수 없다면 “v2 지원”을
유지한 채 현재 빌드로 가짜 픽스처를 만들지 않는다. 과거 v2 생산 근거를
복원하거나, 지원 범위에서 v2를 제거하고 관련 출시 문서를 함께
변경하는 결정이 먼저 필요하다.

## 픽스처 세트

픽스처 파일명은 내용과 경계를 드러내고 서로 덮어쓰지 않게 한다. 아래
여섯 개가 Windows-v1의 최소 세트다.

| ID | 생산 스키마 | 고정 입력 상태 | 소비자 오라클 |
|---|---:|---|---|
| `v1-ch02-legacy` | 1 | CH02의 실제 중단 지점. Chapter, map, checkpoint와 복수의 legacy `StoryStateTags`가 있는 저장 | 로드·맵 이동·체크포인트 재개 성공. Chapter/map/checkpoint와 legacy 태그 집합은 매니페스트와 일치. 외부 버전이 2 미만이므로 REBIRTH 상태는 완전 기본값으로 시작 |
| `v2-ch03-nominal` | 2 | 구매·결제·고양이 선택, truth source, 방문 기록, P3 부분 진행과 P5 관찰이 있는 정상 저장 | 유효한 선택·원자료·진행은 보존되고 중복 이름은 제거됨. 파생 P5 진실과 `NarrativeDebt`는 원자료로 재계산. 로드 뒤 내부 스키마는 3 |
| `v2-normalization-boundary` | 2 | v2 태그의 fixture-only 생산 경로로 만든 잘못된 enum, 범위 밖 P3 값, 모순된 P5·scratch·A/B 상태 | 잘못된 선택 enum은 `Unset`, 잘못된 focused evidence는 `None`. P3 수치와 힌트는 허용 범위로 정규화되고 열리지 않은 유입 상태는 압력 60kPa로 복원. P5 직접 플래그는 원자료로 재계산. 양립할 수 없는 엔딩과 공통 발견은 안전한 선택 전 상태로 되돌아감 |
| `v3-p3-mid-bleed` | 3 | 두 유입 차단, 감압 진행 중, 0이 아닌 압력, 오답·힌트·zero tick 경계 | 밸브, 압력(`±0.01kPa`), 오답 수, 힌트 단계·경과, zero tick이 저장 오라클과 일치. 완료나 본배수가 조기 확정되지 않음 |
| `v3-p5-pre-choice` | 3 | P3 완료, 탱크 개방, P5 관찰 원자료와 네 진실, 세 번째 긁힘 tail 확정, 엔딩 미선택 | P3/P5·truth source·사고 반응과 C5 가능 상태가 보존됨. A/B, 실제 상태 복원, `Found0731`과 강한 큐는 모두 미재생 |
| `v3-ending-common-a` | 3 | Ending A 선택과 공통 발견이 원자적으로 커밋된 직후, 분기 강한 큐 전 | A만 선택·해결되고 B는 없음. `ActualStateRestored`와 `Found0731`은 각각 1회 상태, 공통 발견 커밋은 참. 재저장·재시작 뒤 중복 공통 카드와 B 큐가 발생하지 않음 |

v1 기본 REBIRTH 오라클은 truth, debt, visited/resolved/skipped,
outfit, one-shot 배열이 비어 있고 모든 선택 enum이 `Unset`, upstream
불리언이 false인 상태다. P3는 미완료·60kPa·오답 0·힌트 0, P5와
scratch는 미확정, 엔딩은 `None`이어야 한다.

v2 경계 픽스처는 잘못된 데이터를 만들기 위한 근거 코드가 **v2 생산
태그 안에** 있어야 한다. 현재 v3 구조체를 가져와 임의로 스키마 숫자만
2로 바꾼 파일은 허용하지 않는다. 실제 v2 포맷에 존재하지 않았던 필드는
“기본값으로 추가됨”이라고 오라클에 명시한다.

## 생산 태그와 매니페스트

픽스처별 생산 태그는 annotated tag로 고정하고, tag가 가리키는 40자리
commit, 당시 `.uproject` EngineAssociation과 실제 `Build.version`을
기록한다. v1 후보 근거인
`b9c0fe795d8647cb396b670d632ecea14c564563`도 태그와 원본 Shipping
생산 증거가 생기기 전에는 승인된 출처가 아니다.

보관 구조는 다음과 같다.

```text
SaveCompatibility/
  fixture-set.json
  fixtures/
    <fixture-id>.sav
  oracles/
    <fixture-id>.oracle.json
  producer-logs/
    <fixture-id>.log
  consumer-results/
    <candidate-sha>/
      <fixture-id>.result.json
      <fixture-id>.log
```

`fixture-set.json`에는 최소 다음 값을 넣는다.

```json
{
  "contract": "Save-v1",
  "fixtures": [
    {
      "id": "v1-ch02-legacy",
      "path": "fixtures/v1-ch02-legacy.sav",
      "sha256": "<64자리 SHA-256>",
      "size_bytes": 0,
      "outer_schema": 1,
      "inner_rebirth_schema": null,
      "source_tag": "<불변 annotated tag>",
      "source_commit": "<40자리 SHA>",
      "engine_association": "<uproject 값>",
      "engine_build_version": "<실제 Build.version>",
      "producer_target": "IndieGame Win64 Shipping",
      "producer_exe_sha256": "<64자리 SHA-256>",
      "producer_command_line": "<원문>",
      "produced_at_utc": "<ISO 8601>",
      "oracle_path": "oracles/v1-ch02-legacy.oracle.json",
      "oracle_sha256": "<64자리 SHA-256>"
    }
  ]
}
```

`size_bytes`는 실제 양수여야 하며 placeholder가 남으면 매니페스트 자체가
실패다. tag와 commit이 일치하지 않거나, 생산 로그·EXE 해시·오라클 중
하나라도 빠지면 해당 픽스처는 `BLOCKED`다.

## 오라클 형식

각 oracle JSON은 생산 빌드에서 저장 직전 덤프한 입력과 v3 소비자에서
기대하는 정규화 후 상태를 함께 가진다. 최소 필드는 다음과 같다.

```json
{
  "fixture_id": "v3-p3-mid-bleed",
  "input": {
    "outer_schema": 3,
    "chapter": "Chapter.CH03",
    "map": "<정확한 package name>",
    "checkpoint": "<정확한 gameplay tag>",
    "story_tags": [],
    "rebirth": {}
  },
  "expected_after_load": {
    "outer_schema_accepted": 3,
    "rebirth_schema": 3,
    "chapter": "Chapter.CH03",
    "map": "<동일 package name>",
    "checkpoint": "<동일 gameplay tag>",
    "story_tags": [],
    "rebirth": {}
  },
  "float_tolerance": {
    "pressure_kpa": 0.01,
    "elapsed_seconds": 0.01
  },
  "required_events_in_order": [],
  "forbidden_events": []
}
```

배열 비교는 순서가 의미 있는 이벤트만 순서 비교하고, gameplay tag와
관찰 원자료처럼 집합인 필드는 중복 없는 집합으로 비교한다. truth는
`TruthTag`, `SourceIds`, `bConfirmed` 세 값을 모두 비교한다. enum은
숫자뿐 아니라 이름도 결과에 기록해 레이아웃 변화를 발견한다.

## clean Shipping 소비자 검증

각 픽스처는 같은 출시 후보 SHA에서 아래 절차를 독립적으로 수행한다.

1. G3와 G5를 통과한 Shipping 패키지와 패키지 매니페스트를 clean VM
   스냅샷 또는 개발 도구가 없는 별도 PC에 복사한다. 설치 전 파일 트리,
   `IndieGame.exe`와 PAK/UTOC/UCAS의 SHA-256을 기록한다.
2. 사례마다 VM을 같은 clean snapshot으로 되돌리고 고유 Windows 사용자와
   고유 `-UserDir=<절대 경로>`를 사용한다. 게임이 `-UserDir`을 존중하지
   않으면 사례별 VM 스냅샷을 따로 복제한다.
3. 실행 전에 save/config 폴더가 비어 있고 autosave 두 슬롯이 없으며,
   새 게임의 truth·debt·choice·one-shot·ending 상태가 모두
   zero/unset인지 baseline JSON으로 확인한다.
4. 읽기 전용 원본의 SHA-256을 확인한 뒤 해당 사례의 작업 디렉터리에
   한 개의 `.sav`만 복사한다. 다른 픽스처나 이전 실행의 설정·세이브를
   재사용하지 않는다.
5. Shipping 게임으로 픽스처를 로드하고 맵 이동·체크포인트 재개 후
   oracle 전체를 비교한다. 로드 전, 적용 직후, 맵 이동 후 상태와 이벤트
   순서를 각각 덤프한다.
6. 다른 이름의 v3 슬롯으로 다시 저장하고 정상 종료한다. 프로세스를 새로
   시작해 재저장 슬롯을 로드하고 같은 정규화 상태, one-shot 비중복과
   올바른 최신 autosave 선택을 다시 확인한다.
7. 각 사례가 끝나면 작업 `UserDir` 전체를 삭제하고 경로 부재를 확인한다.
   원본 픽스처 SHA-256을 다시 계산해 실행 전과 같은지 기록한다.

검증용 입력·상태 덤프 인터페이스가 Shipping 후보에 없다면 화면만 보고
추정하지 않는다. 동일 Shipping 코드 경로에서 픽스처 ID, 읽기 전용
fixture 경로와 result JSON 경로를 받는 호환성 하네스를 먼저 구현해야
하며, 디버그 전용으로 마이그레이션 결과를 바꾸는 코드는 금지한다.

## 결과 JSON

각 실행은 `<fixture-id>.result.json`을 원자적으로 작성한다. 최소 형식은
다음과 같다.

```json
{
  "result": "PASS",
  "fixture_id": "v1-ch02-legacy",
  "candidate_commit": "<40자리 SHA>",
  "candidate_exe_sha256": "<64자리 SHA-256>",
  "fixture_sha256_before": "<64자리 SHA-256>",
  "fixture_sha256_after": "<동일 값>",
  "oracle_sha256": "<64자리 SHA-256>",
  "machine": "<OS/CPU/GPU/RAM>",
  "user_dir": "<사례별 절대 경로>",
  "baseline_clean": true,
  "load_succeeded": true,
  "travel_succeeded": true,
  "checkpoint_succeeded": true,
  "oracle_match_after_load": true,
  "resave_as_v3_succeeded": true,
  "restart_reload_succeeded": true,
  "oracle_match_after_restart": true,
  "cleanup_succeeded": true,
  "unexpected_crash_fatal_ensure_error_count": 0,
  "started_at_utc": "<ISO 8601>",
  "finished_at_utc": "<ISO 8601>",
  "artifacts": [
    { "path": "<상대 경로>", "sha256": "<64자리 SHA-256>" }
  ]
}
```

콘솔 출력만 있거나, 실패한 필드를 생략하거나, baseline과 cleanup을
수동 확인으로 표시한 결과는 승인 증거가 아니다. 로그의 Crash, Fatal,
Ensure는 0건이어야 하며 Error는 릴리스 매니페스트에서 원인과 영향이
승인된 항목 외에는 0건이어야 한다.

## 추가 거부 회귀

지원 픽스처와 별도로 다음 두 사례를 실행한다.

- CurrentSchemaVersion보다 큰 외부 버전은 로드를 거부하고 현재 새 게임
  상태와 기존 autosave를 바꾸지 않는다.
- 잘린 파일과 SHA가 다른 파일은 크래시 없이 거부하고 손상 파일 위에
  정상 autosave를 덮어쓰지 않는다.

이 두 파일도 임의로 기존 픽스처를 수정해 지원 픽스처인 것처럼 보관하지
않고 `negative/`에 별도 생성 절차와 해시를 기록한다.

## 통과 기준과 현재 상태

| 항목 | PASS 조건 | 현재 |
|---|---|---|
| v1 | 태그·Shipping 생산 로그·fixture/oracle 해시와 clean 소비자 결과 PASS | BLOCKED — 과거 소스 커밋만 있고 태그·픽스처 없음 |
| v2 | 정상/정규화 경계 두 픽스처의 과거 v2 태그 출처와 소비자 결과 PASS | BLOCKED — v2 생산 태그·바이너리·픽스처 없음 |
| v3 | P3/P5/엔딩 경계 세 픽스처의 v3 태그 출처와 소비자 재시작 결과 PASS | BLOCKED — 태그 픽스처와 clean Shipping 결과 없음 |
| 격리·정리 | 모든 사례의 baseline clean, 원본 hash 불변, 사례별 `UserDir` 삭제 | BLOCKED — 미실행 |
| 거부 회귀 | 미래 버전·손상 파일이 상태 변경과 크래시 없이 거부됨 | BLOCKED — 미실행 |

여섯 지원 픽스처와 두 거부 회귀가 같은 출시 후보에서 모두 PASS하고
fixture-set, oracle, result, 패키지 파일의 SHA-256이 연결돼야 G6 세이브
호환성을 통과한다. 하나라도 누락되면 `PARTIAL`이 아니라 `BLOCKED`,
오라클 불일치·크래시·상태 오염은 `FAIL`이다.
