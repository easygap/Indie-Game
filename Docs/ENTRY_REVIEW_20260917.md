# 403호와 관리실 소품 점검 — 2026-09-17

수정 전 기준은 `36a0e0104972e9e61303d672600f1d121876aef6`이다. 9월 16일까지 공개된 제작 자료와 실물 사진을 조사하고, 기존 캡처를 둘러본 뒤 문제가 확인된 자리를 실제 게임에서 다시 찍었다.

## 확인한 문제

403호 인터폰에는 화면이 그려진 판 위로 검은 화면이 한 겹 더 붙어 있었다. 외함도 참고 제품보다 컸다. 현관 옆 청록색 물체는 작업조끼였는데, 상자에 간판용 발광 재질을 써서 문과 벽까지 푸르게 비췄다.

방의 소품에는 이전 이야기의 설정도 남아 있었다. 현재 주인공은 백유담인데 수험 플래너, 새벽 출근 일정, 어머니의 문자와 지갑 챙기기가 그대로 생성됐다. 이 소품 묶음과 상자 모양 공책은 현재 게임에서 만들지 않도록 했다. 보관 중인 이전 이야기에서는 계속 사용한다.

책상에는 또 다른 문제가 있었다. 입주 계약서 아래로 이전 공책이 겹쳤고, 셋째 밤 배송 단서는 글씨 없는 1.2cm 두께의 상자였다. 공책을 없애자 계약서가 공중에 남았다. 계약서를 실제 상판 높이에 맞춰 내리고, 배송 라벨을 왼쪽 빈자리로 옮겼다. 라벨은 인쇄된 얇은 종이 세 장으로 교체했다. 이름·주소·보관 요청은 읽기 화면의 문구와 맞췄다.

## 실물 참고와 제작

| 대상 | 확인한 자료 | 반영한 부분 |
| --- | --- | --- |
| 인터폰 | [코콤 KCV-R431E 제품 정보](https://www.kocom.com/product/view.php?c_no=17&p_no=17), [실물 상품 사진](https://www.sionmart.co.kr/goods/goods_view.php?goodsNo=1000000053) | 140×203×26mm 규격, 얇은 외함, LCD 한 개, 스피커와 터치 버튼 위치 |
| 작업조끼 | [한국 3M 안전용품 카탈로그](https://multimedia.3m.com/mws/media/2208839O/3m-psd-pdf.pdf), 인쇄면 109쪽 | V자 목둘레, 빈 암홀, 앞뒤 원단, 반사띠, 지퍼와 옷걸이 |
| 배송 라벨 | [정책브리핑의 실물 운송장 사진, 2024-10-04](https://www.korea.kr/news/reporterView.do?newsId=148934239) | 얇은 점착지, 구획선과 인쇄 위계, 박스에서 떼어 낸 가장자리 |

코콤 제조사 페이지의 규격은 검색 색인에서 확인했다. 실물 이미지는 판매처 사진을 사용했다. 외부 사진을 게임 텍스처로 복사하지 않고 생성 참고로 사용했다.

내장 `image_gen`으로 [인터폰 다각도 시트](../Content/SourceArt/AI/IntercomReference_20260917.png), [조끼 다각도 시트](../Content/SourceArt/AI/WorkVestReference_20260917.png), [배송 라벨 인쇄 원본](../Content/SourceArt/AI/ShippingLabel_20260917.png)을 만들었다. Blender에서 시트와 실물을 대조해 메시를 만들고 UE 에셋으로 반입했다. 라벨은 생성한 인쇄 원본을 윗면에만 구웠다.

사용한 프롬프트와 출처는 [인터폰·조끼 기록](../Content/SourceArt/AI/EntryReference_20260917.json), [라벨 기록](../Content/SourceArt/AI/ShippingLabel_20260917.json)에 있다. 생성 이미지가 아래 게임 캡처를 대신하지 않는다.

조끼는 관리실 자재 보관 벽에 걸었다. 처음 반입한 위치에서는 배관과 겹쳐 보여 반대쪽 벽으로 옮겼고, 아래 판재와도 간격을 뒀다. 조끼와 인터폰 모두 발광을 없앴다.

## 실제 실행 화면

UE 5.8, 1920×1080, High 설정의 실행 화면이다. 전후 비교는 같은 카메라와 조명을 사용했다. 근접 시점은 두께·인쇄·겹침을 확인하기 위한 고정 카메라다.

| 수정 전 | 수정 후 |
| --- | --- |
| ![화면이 겹친 인터폰과 푸르게 빛나던 현관](Media/readme/entry-before-room.webp) | ![규격을 맞춘 인터폰과 정리한 현관](Media/readme/entry-room.webp) |
| ![책상에 겹쳐 있던 이전 소품과 단서](Media/readme/entry-before-desk.webp) | ![계약서와 배송 라벨을 떨어뜨려 놓은 책상](Media/readme/entry-desk.webp) |

![관리실 벽에 걸린 작업조끼](Media/readme/entry-vest-installed.webp)

![게임 속 배송 라벨의 인쇄](Media/readme/entry-labels-front.webp)

[인터폰 정면](Media/entry-intercom-front.png) · [측면](Media/entry-intercom-side.png) · [방 건너편 거리](Media/entry-intercom-far.png) · [조끼 옆면](Media/entry-vest-angle.png) · [관리실 자재와의 간격](Media/entry-booth-room.png) · [종이 두께](Media/entry-labels-side.png)

## 조사 흐름과 개발 자료

[Silent Hill: Townfall 제작진의 2026-09-16 설명](https://blog.playstation.com/2026/09/16/silent-hill-townfall-creators-break-down-ps5-features-out-september-25/)에서는 주민들의 흔적, 환경에 놓인 이야기 조각, 탐색과 퍼즐의 관계를 다룬다. 이번에는 이를 단서와 생활 소품의 맥락을 점검하는 기준으로 삼았다. 다른 주인공의 생활사가 섞인 물건을 걷어 내고, 배송 단서의 실제 표면과 읽기 화면을 일치시켰다.

첫날 읽은 계약서·소포·수첩·음성메시지·402호 메모는 다시 확인할 수 있다. 한 번 읽자마자 상호작용이 없어져 기억에 의존해야 했던 부분을 바꿨다. 재확인은 이미 읽은 기록을 중복 진행으로 세지 않는다. 필요한 단서를 아직 다 읽지 않았다면 취침도 열리지 않는다.

[UE의 Lumen 설명](https://dev.epicgames.com/documentation/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine)은 발광 표면이 주변 간접광에 영향을 주는 방식을 다룬다. 조끼의 간판 재질을 제거한 이유도 이 때문이다. 반사띠는 주변 빛을 받는 표면으로 만들었다.

[UE의 LOD 설명](https://dev.epicgames.com/documentation/unreal-engine/creating-and-using-lods-in-unreal-engine)에 맞춰 새 소품에도 화면 점유율에 따른 기존 4단계 LOD를 적용했다. 조끼는 저작 중 4,692개였던 삼각형을 2,657개로 줄인 뒤 구웠다. 앞판의 반사띠 경계는 유지하고 뒤판 격자와 가는 봉제선의 밀도를 줄였다. 인터폰은 1,728개, 라벨은 48개다. 각각 재질 한 개와 1K 색·노멀·ORM 텍스처를 사용한다. 조끼는 고정 메시이며 천 시뮬레이션을 돌리지 않는다.

[반입 에셋 검사 결과](Performance/Entry20260917-assets.json)에서 세 메시의 실제 삼각형 수, 4단계 LOD, 재질 슬롯 하나, 발광 0, 텍스처 스트리밍을 확인했다. 위 수치는 Blender 원본 기준이며, UE 반입 뒤 LOD0은 인터폰 1,520개·조끼 2,633개·라벨 48개다.

## 다른 구역

`Run-RetailReview.ps1 -Spatial`로 21개 시점을 다시 찍었다. 편의점 직원의 얼굴과 목, 상품 앞뒤 인쇄, 계산대, 전자레인지, 복도·매장 타일, 관리실 출입구와 자재, 옥상 밸브를 확인했다. 이 구역의 기존 에셋을 이번에 모두 새로 만든 것은 아니다.

[편의점 직원](Media/retail-clerk-left.png) · [상품과 가격표](Media/retail-stock.png) · [매장 줄눈](Media/spatial-store-floor.png) · [전자레인지](Media/kitchen-microwave.png) · [관리실 출입구](Media/spatial-booth-entry.png) · [옥상 밸브](Media/spatial-roof-valves.png)

## 검증과 성능

| 검사 | 결과 |
| --- | --- |
| UE 5.8 Development Editor 빌드 | 성공 |
| 프로젝트 필수 검사 | `Scripts/Validate-Project.ps1` 통과 |
| 입주 구간 | 문·통로 24곳 통과, 계약서 접지·라벨 분리·읽기 판정 통과 |
| 단서 재확인 | 다섯 단서 재열람, 중복 진행 0, 이전 생활 소품 생성 0 |
| 전체 greybox 실행 | 전력 퍼즐·CCTV·추격·응답 노크·넷째 밤·엔딩 A·실패 후 재시도·저장 복원·오디오 검사 통과 |
| 60fps 조작 검사 | 이동·상호작용·문 너머 잡기 차단·잡혔을 때 몸과 얼굴 위치 검사 통과 |
| 실물 소품 반입 검사 | 세 메시의 4단계 LOD·삼각형 예산·재질·스트리밍 통과 |
| 화면 검수 | 403호·관리실 14개 시점, 나머지 동선 21개 시점 |

성능 비교는 같은 9개 시점에서 별도로 진행했다. 이미지 저장을 끄고 VSync와 프레임 제한을 해제했으며, 처음 120프레임을 제외했다. Ryzen 9 7900X·RTX 3060 8GB, D3D12, 1080p High, 내부 해상도 100% 조건이다. 방과 관리실을 오가는 구간의 측정이며 다른 사양이나 모든 장면의 프레임을 보장하는 수치는 아니다.

| 항목 | 수정 전 | 수정 후 | 수정 후 재측정 |
| --- | ---: | ---: | ---: |
| 분석 프레임 | 2,694 | 2,762 | 2,532 |
| 프레임 시간 중앙값 | 16.715ms | 16.619ms | 16.616ms |
| 프레임 시간 p95 | 21.907ms | 20.802ms | 22.978ms |
| GPU 시간 p95 | 19.202ms | 18.791ms | 18.487ms |
| 게임 스레드 시간 p95 | 2.761ms | 3.198ms | 2.612ms |
| 드로 콜 p95 | 721 | 666 | 666 |
| GPU 메모리 p95 | 3,107MiB | 3,156MiB | 3,159MiB |
| 33.33ms 초과 프레임 | 62개 (2.30%) | 29개 (1.05%) | 87개 (3.44%) |
| 최대 프레임 시간 | 86.040ms | 189.229ms | 92.949ms |

p95는 전체 프레임의 95%가 그 값 안에 들어온다는 뜻이다. 드로 콜은 줄었지만 프레임 시간에는 실행 간 편차가 있어서 속도가 확실히 빨라졌다고 결론 내리지 않았다. 새 소품을 넣은 뒤 GPU 메모리 p95는 약 49~52MiB 늘었다.

수정 후 첫 측정의 13.30초 지점에서 GPU 지연과 함께 189ms 프레임이 한 번 기록됐다. 인터폰 측면을 보고 있을 때였고 카메라 전환 시점은 아니었다. 재측정에서는 100ms 초과 프레임이 없었다. 긴 끊김의 원인은 이 CSV만으로 특정하지 못했으며, 해당 프레임을 빼지 않고 원본에 보존했다.

[수정 전 원본·요약](Performance/Entry20260917-before-high/) · [수정 후 원본·요약](Performance/Entry20260917-after-high/) · [재측정 원본·요약](Performance/Entry20260917-after-high-repeat/) · [실행 검사 로그 발췌](Performance/Entry20260917-checks.txt)

검수 화면은 `pwsh -NoProfile -File Scripts/Run-EntryReview.ps1`, 성능 기록은 같은 명령에 `-Measure -Quality High -Profile review`를 붙여 재현할 수 있다. 수정 전 기록은 위 기준 커밋에서 같은 카메라 검사 코드를 사용해 남겼다.
