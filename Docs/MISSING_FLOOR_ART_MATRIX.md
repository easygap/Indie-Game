# 없는 층 — ImageGen 에셋 적용 매트릭스

> 기준일 2026-08-11. 이 문서는 이미지 목록이 아니라 **어떤 원본을 어떤
> 런타임 표현으로 바꾸고, 어디까지 접근을 허용할지**를 잠그는 실행 계약이다.

## 적용 원칙

1. 가까이 갈 수 있거나 손전등 각도가 바뀌는 사물·인물·배경은 3D/PBR이다.
2. 손자국·먼지처럼 본질적으로 얇은 것은 값 마스크로 기존 표면에 블렌드한다.
3. 일반 인물 스프라이트는 12m 이상, 접근 불가, 한 컷에만 쓴다. 위층
   사람은 예외적으로 연속 3D 접지 셸을 유지한 채 1.6m 정면에서 PBR
   가독성 레이어를 켠다. 활성화 뒤 1.25m까지 유지하되 측면에서는 셸로
   되돌린다.
4. ImageGen에 조명·그림자·한글·증거 상태를 굽지 않는다. 조명과 그림자는
   UE, 의미 있는 한글은 런타임 UI, 퍼즐 상태는 코드가 소유한다.
5. 원본 시트는 카메라에 직접 노출하지 않는다. 크롭·알파·PBR·메시 변환을
   거친 파생 에셋만 Shipping 경로에 들어간다.

## Keep / 3D / PBR / Sprite / Blend

| 대상 | ImageGen 원본 | 런타임 에셋 | 방식 | 실제 적용 | 합격 기준 |
|---|---|---|---|---|---|
| 불법 5층 배경 | `SheetMissingFloorEnvironmentReference.png` | `AIGPrologueWorldScene::BuildFifthFloorAnnex` | Keep geometry + reference | 철골·옥상·복도 비례와 노출 기준 | 배경 사진·가짜 출입구 0, 충돌과 보이는 면 일치 |
| 건식 석고 | `TextureMissingFloorDryPlaster.png` | `T_MissingFloorDryPlaster_{D,N,R,A}`, `M_MissingFloorPlaster_X/Y/XY` | PBR | 5층 벽·천장 | 이음선 0, 금속성 0, 거칠기 0.88 이상, 조명 베이크 0 |
| 손자국·끌림·분진·긁힘 | `SheetMissingFloorResidueMasks.png` | 마스크 4장 + 전용 masked material 4종 | Blend | 5층 벽·바닥·공동 안쪽 | 사각 테두리 0, 표면 이격 0.15cm, 그림자·충돌 0 |
| 위층 사람 | `SheetListenerEntityAnatomyReference.png`, `ListenerEntityFrontCutout.png`, `SheetListenerEntityCrawlPhases.png` | `SM_ListenerEntityCrawl`, `M_MissingFloorListenerPlasterUV`, `T_SpriteListenerCrawl0..3_{D,N,R,A}`, `M_SpriteListenerCrawl0..3` | 3D shell + distance/angle PBR sequence | 1.6m 정면에서 4단계 레이어 활성화, 1.25m까지 유지, 측면은 3D | 128×85cm 투영, 프레임별 초록 프린지 0, 1.6~6fps 속도 연동, 바닥 접지, 숨은 3D 그림자; 측면 평면 노출 0 |
| 서일영 | `SheetMissingFloorDistantCharacters.png` 좌상 | `T_SpriteSeo_D`, `M_SpriteSeo` | Sprite | 낮3 골목 건너편 1회 | 12m 이상, 접근 불가, 발 알파가 지면과 일치, 8m 내 숨김 |
| 목한수·황순금·나린 | 같은 시트 나머지 | 각 `T_Sprite*_D`, `M_Sprite*` | Prepared sprite | 고정 원거리 큐가 생길 때만 배치 | 근접 대화 대용 금지. 현재는 미배치가 정답 |
| 조율 렌치 | `SheetMissingFloorHeroPropsReference.png` 우상 | `SM_TuningHammer` | 3D hero prop | 밤3 자재 위, P3·엔딩 인과 | 27cm L자형, 실제 접촉 그림자, 원통형 청음봉 폴백은 개발 전용 |
| 조율 공구 카트 | 같은 시트 좌상 | `SM_TunerToolCart` | 3D prop | 5층 서북 모서리 | 45×34×78cm, 선반 틈·바퀴·손잡이 시차, 바닥 관통 0 |
| 민원 원장·먹지 | 같은 시트 좌하 | `SM_ComplaintLedger` | 3D hero paper | 관리실 책상 Z=76cm 위 | 2.8cm 실제 두께, 중심 Z=77.5cm, 접촉 그림자, 공중 부양 0 |
| 달력 뒷장 소리 일지 | 같은 시트 우하 | `SM_CalendarJournal` | 3D hero paper | 401호 문 앞 전달 큐 | 종이판·상단 바인딩 실제 기하, 글은 런타임 패널, 가짜 글자 0 |
| 1인칭 두드리기 오른손 | `SheetFirstPersonKnockPhases_v2.png` | `T_FPHandKnock0..3_D` | UI-space RGBA sprite blend | Q/B 유효 노크의 준비·접촉·반동 0.22초 | 같은 손·소매 유지, 입력 프레임에 접촉(2), 소매 끝은 화면 밖, 초록 프린지 0, 흔들림 감소 시 이동 0, 월드 평면 0 |
| 1인칭 포획 포옹 | `SheetListenerCaptureEmbracePhases_v1.png` | `T_FPCaptureEmbrace0..3_D` | UI-space RGBA sprite blend | 포획 암전의 접촉·접근·닫힘·유지 1.2초 | 같은 두 팔·건식 석고·낡은 옷, 얼굴/몸통 0, 중앙 35% 가독, 화면 밖 소매 끝, 초록 프린지 0, 흔들림 감소 정지 프레임, 월드 평면 0 |

## 블렌딩·거리·성능 계약

- 건식 석고는 D/N/R/A를 한 재질에서 샘플하고 X/Y/XY 세 방향 인스턴스로
  공유한다. 5층 표면마다 별도 4K 재질을 만들지 않는다.
- 흔적 평면은 최대 4종, 한 화각 동시 노출 최대 3장이다. 투명 반투명 대신
  masked를 사용해 정렬 문제와 과도한 오버드로를 막는다.
- 서일영 스프라이트는 낮 구간 한 장만 활성화하고 나머지 셋은 비가시 상태다.
  8m 안쪽 숨김, 12m 이상 표시, 40m 컬링을 기본값으로 삼는다.
- 위층 사람 정면 레이어는 1.6m·정면 내적 0.60에서 켜지고 1.25m 또는
  내적 0.35 아래에서 꺼지는 히스테리시스를 쓴다. 보이는 레이어는 masked
  D/N/R/A로 수광하고, 이동 속도에 따라 네 자세를 1.6~6fps로 순환한다.
  정지하면 프레임을 유지하며 응답 노크 대기 상태는 중앙 지지 자세로
  고정한다. 숨은 연속 셸만 실제 접지 그림자를 낸다.
- `SM_ListenerEntityCrawl`, 조율 렌치, 원장, 일지는 서사 근접 판독 때문에
  LOD0를 보존한다. 카트는 SmallProp LOD 그룹을 쓰며 화면 점유율 2% 아래에서
  단순화한다. 불법 5층 구조물은 반복 블록 재질을 공유하고 개별 Tick이 없다.
- 스프라이트 알파, 흔적 값 마스크, 석고 PBR은 모두 1024px 이하 파생본을
  사용한다. 원본 1254px 시트는 SourceArt 증빙이며 런타임에 로드하지 않는다.
- 1인칭 손은 투명 여백을 포함한 768px RGBA 네 장을 UI 그룹·NoMip·Clamp·NeverStream으로
  임포트한다. 문·벽·인물·동물·배경은 기존 3D/PBR을 유지하며, 손 이미지를
  월드 카드나 충돌 대용으로 쓰지 않는다.
- 포획 포옹은 1024px RGBA 네 장을 같은 UI 설정으로 임포트한다. 화면의 긴
  변으로 정사각 프레임을 오버스캔해 종횡비를 보존하고 0.72 정규화 시점까지
  네 자세를 한 장씩 전환한 뒤 암전까지 마지막 자세를 유지한다. 포즈 간
  교차 페이드는 팔이 네 개로 보이므로 금지한다. 장면과의 알파 블렌딩만 쓰며,
  월드의 위층 사람 셸과 포획 지점 손자국 masked 블렌드는 계속 별도 3D 표면으로 남는다.

## 물리·화면 승인

- 프롭 바닥/책상 접지 오차 1cm 이하, 접촉 그림자 중심 이탈 2cm 이하.
- 손전등을 좌우 60도로 움직여 존재·렌치·카트에 시차와 셀프 섀도가 남아야 한다.
- 흔적을 비스듬히 볼 때 카드 두께나 사각 외곽이 보이면 실패다.
- 원거리 인물은 720p에서 사람으로 읽히고 1080p에서 마젠타 프린지 픽셀 0,
  지면 아래 알파 누락 0이어야 한다.
- 위층 사람은 1920×1080 이동 캡처에서 복도 벽 관통 0, 초록 프린지 0,
  4단계 자세 중복 0, 실제 전진, 사람 판독 성공, 1.25m 경계 전환 뒤 카드
  노출 0이어야 한다.
- 캡처 한 장이 아니라 이동 전후 두 프레임과 그림자 프레임을 함께 승인한다.
- 포획은 16:9·16:10·21:9에서 두 팔의 종횡비 변화 0, 화면 안쪽 소매 절단면
  0, 중앙 시야 유지, 1.2초 뒤 잔류 프레임 0을 승인한다.

## 재현 경로

```powershell
.\Scripts\Build-ArtAssets.ps1 -MissingFloorOnly
.\Scripts\Build-ArtAssets.ps1 -HudUiOnly
.\Scripts\Test-ArtAssetContract.ps1
.\Scripts\Run-MissingFloor-NightCapture.bat
```

첫 명령은 ImageGen 원본 분리 → PBR 생성 → 메시 베이크 → 텍스처/머티리얼
UAsset 생성 → 링크 감사를 헤드리스로 수행한다. 두 번째 명령은 원본·파생·
런타임 바인딩과 스프라이트 사용 경계를 정적으로 잠근다.
