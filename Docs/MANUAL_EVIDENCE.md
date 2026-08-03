# REBIRTH 수동 플레이 증거 덤프

## 목적

R1~R4와 S4를 실제 입력으로 검증할 때 영상·세이브 해시와 함께 현재
내러티브 상태를 남긴다. 이 덤프는 수동 실행을 관찰하기 위한 자료이며,
파일이 생성됐다는 사실만으로 G3나 출시 게이트를 통과 처리하지 않는다.

평소 실행에서는 계측 타임라인과 파일 쓰기가 모두 꺼져 있다. 아래 인자나
콘솔 명령을 사용한 QA 세션에서만 수집한다.

## 실행

세션 시작부터 P4와 엔딩 순서를 수집하려면 실행 인자에 캡처 스위치를
넣는다.

```text
-IGRebirthEvidenceCapture
-IGRebirthEvidencePath="D:\REBIRTH-Evidence\R1"
```

첫 상태와 종료 상태는 콘솔에서 각각 덤프한다.

```text
ig.Rebirth.DumpEvidence R1-start
ig.Rebirth.DumpEvidence R1-final
```

`-IGRebirthEvidencePath`를 생략하면
`Saved/Validation/RebirthManual/<label>.json`에 기록한다. 상대 경로는
항상 이 폴더 아래에서 해석한다. 두 번째 콘솔 인자로 명시한 절대 경로는
그대로 사용한다.

```text
ig.Rebirth.DumpEvidence R2-final "D:\REBIRTH-Evidence\R2\final.json"
```

콘솔을 직접 열지 않는 실행은 표준 `-ExecCmds`로 시작 덤프를 호출할 수
있다.

```text
-IGRebirthEvidenceCapture
-IGRebirthEvidencePath="D:\REBIRTH-Evidence\R3"
-ExecCmds="ig.Rebirth.DumpEvidence R3-start"
```

`-IGRebirthEvidenceAutoDump`를 추가하면 P4 관찰과 S4 마일스톤 직후에도
자동으로 저장한다. 출력 경로가 디렉터리면
`event-001-<event>.json`처럼 각 상태를 보존한다. 출력 경로가 `.json`
파일이면 같은 파일을 최신 상태로 원자 교체한다. 수동 경로의 최종 덤프는
영상 종료 직전에 별도로 남기는 것을 원칙으로 한다.

## JSON 계약

덤프는 다음 상태를 한 파일에 포함한다.

| 영역 | 기록 내용 |
|---|---|
| `convergence` | C1~C6 현재 합류 가능 여부 |
| `narrativeDebt` | 아직 회수하지 못한 진실 태그 |
| `choices` | A/B/C 구매, 결제 수단, 고양이 물, 병마개, 종이컵·대기 |
| `inventory` | 기억 손전등, 탱크 조기 개방 |
| `history` | 방문 장소, 해결·명시 생략 퍼즐, 착의 장, 원샷 |
| `puzzles.P1~P5` | 관찰·확정·경로 생략, P3 압력/오답/힌트, P4 하강 횟수/상행 확인, P5 원자료/네 결론/긁힘 |
| `truths` | 모든 진실의 확인 여부와 정렬된 원자료 출처 |
| `ending` | 배타적 선택, 실제 상태 복원, 7월 31일 발견, 강한 큐, 구조화 타임라인 |
| `legacyStoryTags` | 호환 이벤트 버스의 전체 태그 |

P1·P2의 `explicitlySkipped`는 스냅샷에 직접 기록된 값이고,
`routeSkipped`는 C3에 합류했지만 해당 프록시를 해결하지 않은 경로를
뜻한다. P3도 같은 방식으로 옥상 합류 시점을 기준으로 경로 생략을
구분한다.

P4는 기존 v3 스냅샷에 전용 영속 필드가 없으므로 활성화된 한 실행 안에서
하강 루프 횟수와 상행 진입을 별도 관찰한다. 직접 위로 올라간 경로는
`downwardLoopCount=0`, `observed=false`, 옥상 도달 뒤
`skipped=true`로 남는다. P4 중간에 프로세스를 종료한 뒤 재개하는 검증은
영상과 세이브 해시를 함께 보고 판정해야 한다.

## S4 순서

양 엔딩의 완전한 신규 실행은 `ending.timeline`에 다음 순서를 정확히 한
번씩 남겨야 한다.

1. `Choice`
2. `ActualStateRestored`
3. `Found0731`
4. `CommonDiscoveryCard`
5. `BranchCoda`

`timelineCompleteInOrder=true`와 `timelineDuplicateFree=true`를 함께
확인한다. 각 항목에는 당시 엔딩 선택과 공통 발견 상태도 들어간다.
로드 재개처럼 세션 중간에서 캡처를 시작한 파일은 전체 prefix가 없으므로
`timelineCompleteInOrder=false`가 정상일 수 있다. 이 경우 앞 세션의
덤프·세이브 해시·무편집 영상을 이어서 판정한다.

## 결정성과 파일 안전성

- 실행 시각, 프레임 시각, 임의 식별자는 JSON 본문에 넣지 않는다.
- 태그·이름·원자료 배열과 진실 목록은 저장 전에 정렬한다.
- 같은 상태와 같은 label은 같은 JSON 본문을 만든다.
- UTF-8 임시 파일을 최종 파일과 같은 폴더에 쓴 뒤 교체한다.
- `REBIRTH_EVIDENCE PASS <absolute path>` 로그가 있어야 성공으로 본다.
- 정상 실행에는 계측 상태 변경, 자동 덤프, 디스크 쓰기가 없다.

정적 연결 계약은 다음 명령으로 확인한다.

```powershell
.\Scripts\Test-Rebirth-EvidenceContract.ps1
```

이 검사는 명령 진입점, 출력 경로, 결정적 정렬, P1~P5 필드, S4 훅과
비활성 차단을 검사한다. UHT/UBT 빌드와 실제 실행을 대신하지 않는다.
