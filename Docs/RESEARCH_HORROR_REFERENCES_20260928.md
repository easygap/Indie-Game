# 공포 연출·착시 최적화 조사 — 2026-09-28

세 갈래로 조사했다. 괴물과 소품을 어떻게 디자인하는지, 언리얼 5에서 3D처럼
보이게 속이는 기법이 무엇이고 실제로 어디서 프레임이 새는지, 2024~2026년
공포 게임이 무엇을 하고 무엇에 지쳤는지. 대괄호 번호는 아래 출처 목록이다.
검색 요약만 보고 본문은 열지 못한 출처는 목록에 따로 표시했다.

## 이번에 넣은 것

**형광등 떨림이 그의 거리를 알려 준다.** Amnesia: The Bunker는 괴물이 가까우면
등이 깜박이게 해서, 무섭지만 공정한 신호로 썼다 [C9]. 4층 복도의 떨림은 그가
가까울수록 잦고 깊어진다. 끊길 확률이 최대 0.30 오르고, 끊겼을 때 밝기는 0.2배까지
떨어진다. 캡처나 연출이 등을 쥐고 있을 때는 끼어들지 않는다.

**문 안쪽의 이웃.** 2025년 KCI 논문은 한국 주거 공포의 중심을 문과 침입에 두고 [T30],
실제 뉴스에도 우유 투입구로 집 안을 엿듣던 사람이 있었다 [T32]. 봉인된 04:30에
401호 문 아래로 전구색 빛줄이 새고, 문 앞에 처음 서면 안쪽에 선 발이 빛줄을 끊었다가
옮기고 불이 7.5초 꺼진다. 한 판에 한 번이다. 형광등과 색온도를 다르게 해야 문 아래가
따로 읽힌다.

**옥상 원경은 무한히 먼 그림이다.** 조사 중에는 원경 판을 두세 겹으로 나눠 판 사이
시차로 깊이를 내자는 안이 나왔다. 해 보니 34 m 앞의 판은 옥상을 걸을 때 무대
배경처럼 같이 밀렸고, 판 네 장 사이 모서리마다 하늘이 뚫렸다. 그래서 판을 지도를 닫는
상자로만 쓰고, 그림은 재질이 시선 방향으로 찾게 바꿨다. 스카이박스와 같은 원리라
원경은 움직이지 않고 가까운 난간·물탱크·이웃 빌라만 그 앞을 지나간다. 이 대비가
거리감을 만든다. 상자가 2 cm 두께라 그림자·거리장·Lumen 장면에 들어가지 않는다.

**창마다 켜지고 꺼지는 동네.** 원경과 창밖 사진의 불 켜진 창을 덩어리로 찾아 창마다
문턱값을 주고, 게임이 「깨어 있는 집」 값을 내려보낸다. 창마다 따로 켜지고, 몇 집은
TV만 켜 둔 것처럼 푸르게 깜박인다. 입주한 저녁에는 대부분 켜져 있고,
04:30에는 열 집에 한 집꼴, 15분에 걸쳐 새벽 출근하는 집부터 하나둘 켜진다.

**새벽이 오다 멈춘 하늘.** 7월 말 서울의 해돋이는 5시 반 무렵이라 04:30은 해 뜨기
한 시간 전이다. 도시 불빛이 지평선을 탁하게 띄우고 동쪽 끝에만 푸른 기가 걸린다.
이 띠가 없으면 원경 건물이 하늘과 같은 검정이라 창 불빛만 점점이 보였다. 띠를 더하자
윤곽이 하늘을 등지고 읽힌다. 하늘은 더 밝아지지 않는다.

**창 너머는 평면 한 장이다.** 유리 뒤 10 m에 사진 평면을 두고 광선을 보내는 한 장짜리
시차로, 인테리어 매핑을 가장 단순하게 줄인 형태다 [U18, U21]. 예전 식은 북향 창만
셈해서 남향 복도 창에서는 사진 모서리 한 점으로 뭉개졌고, 비스듬히 보면 가장자리 한
줄이 가로 줄무늬로 늘어났다. 보는 방향의 부호를 곱하고 가장자리를 q/sqrt(1+q²)로 눌러
사진 안에 담는다. 침실 창도 예전에는 창짝마다 사진이 통째로 붙어 같은 풍경이 두 번
반복됐는데, 이제 건너편 빌라의 벽이 두 창짝에 이어져 보인다.

**보이지 않는 층의 등을 끈다.** 이 정도 크기의 씬에서는 도형보다 조명·그림자·안개가
프레임을 먹을 것이라고 봤고 [U5, U6, U14], 실측도 같았다. GPU에서 조명이
6.2 ms로 가장 컸다. 카메라 높이로 1~2층·4층·옥상 세 구역을 나눠 다른 층의 등을 숨긴다.
측정 경로 전체의 프레임 시간 중앙값이 18.7 ms에서 16.7 ms로, 드로 콜이 599에서 506으로
줄었다. 구역을 끄고 잰 §V5 값이 소수 넷째 자리까지 같아서 화면은 바뀌지 않는다.
관리실 CCTV 5번처럼 다른 층을 따로 렌더하는 화면이 살아 있는 동안에는 모든 층을 켜
두고, 4일 차 밤에 내려간 별관 차단기는 층을 오르내려도 되살리지 않는다.

## 일부러 넣지 않은 것

- 바라보면 멈추는 괴물. 그는 보지 못한다. 그의 정지는 듣는 자세이고, 사일런트 힐 2
  리메이크의 마네킹은 숨는 자리가 읽히자 우스워졌다 [C6, C10].
- 눈의 반사광. 얼굴이 없다. 눈에 빛이 없는 얼굴이 죽은 것처럼 읽힌다는 촬영 쪽 이야기가
  오히려 지금 설계를 받쳐 준다 [C15].
- 강한 SSS. 손전등처럼 센 빛에서 띠가 생기고 역광 산란이 없다 [C13]. 마른 석고 몸이라
  거칠기를 높게 둔다 [C12, C14].
- MegaLights. 그림자를 드리우는 등이 한 화면에 몇 개 없어서 고정 비용만 는다 [U9].
- 필름 그레인·수차·비네트 겹쳐 바르기. 낮은 완성도를 가리는 용도로는 쓰지 않는다 [C33].

## 다음 후보

- 이웃 빌라 창의 인테리어 매핑. 방 사진 한 장을 원근 보정해 박스 안에 넣는 수식이
  조사에 있다 [U18, U20, U21]. 이웃 상자에 창 칸 UV부터 필요하다.
- 계단참 센서등. 스스로 켜지는 센서등이 무섭다는 글이 많고, 마이크로파 센서는 얇은 문
  너머에서도 켜져 30~60초 간다 [T25, T26]. 없어야 할 5층 계단참에서 딸깍 켜지는 등.
- 기는 자세의 불규칙한 멈춤. 몸의 박자는 어긋나게, 노크 세 번과 8초 듣기는 정확하게
  [C2, C3, C4]. 속도가 붙기 전에는 반드시 석고 갈라지는 소리를 먼저 [C8, C9].
- 새벽 매미, 모기향, 문에 붙은 광고 스티커와 뗀 자국 같은 7월 말 빌라의 생활 디테일.
  괴물보다 일상이 정확해야 초자연이 선다 [C2].
- 낮 장면의 창밖. 지금은 낮에도 밤 사진을 어둡게 쓴다.

## 배포 전에 할 일

Steam은 2026년 1월 양식부터 게임에 들어가거나 상점 페이지에 쓰인 생성형 AI 결과물을
밝히게 한다 [C26]. 이 게임에는 GPT Image 원본과 그것으로 만든 TRELLIS 메시가 들어간다.
대상 목록은 `ASSET_POLICY.md`의 생성형 이미지 표와 날짜별 기록이다. Clair Obscur는 QA가
놓친 임시 AI 텍스처 때문에 인디 게임 어워드 두 부문을 돌려줬다 [C25]. 임시 그림이
남지 않았는지 배포 전에 표와 대조한다.

## 출처

괴물·소품 디자인(C)

- C1 https://www.superjumpmagazine.com/the-making-of-amnesia-the-bunker/
- C2 https://www.creativebloq.com/3d/video-game-design/how-unreal-engine-5-3-made-still-wakes-the-deep-more-terrifyingly-beautiful (재게재본 https://tech.yahoo.com/general/articles/still-wakes-deep-made-more-100000031.html 으로 읽음)
- C3 https://www.gamesradar.com/games/horror/still-wakes-the-deeps-creature-movements-were-created-by-a-technical-glitch-and-they-were-so-good-it-immediately-unnerved-everyone/ (제목만)
- C4 https://bogleech.com/halloween/hall25-silenthillf
- C5 https://80.lv/articles/how-ill-combines-body-horror-physics-and-binaural-audio-to-terrify-players
- C6 https://www.siliconera.com/interview-silent-hill-f-inspirations-and-designs/
- C8 https://www.keengamer.com/articles/features/opinion-pieces/the-art-of-scary-monster-design/
- C9 https://www.aiandgames.com/p/how-the-beast-works-in-amnesia-the
- C10 https://www.thegamer.com/silent-hill-2-mannequin-legs-scooby-doo-slapstick/
- C11 https://geekculture.co/alien-isolation-2-keeps-xenomorph-scary-unpredictable-in-an-open-world/
- C12 https://pmc.ncbi.nlm.nih.gov/articles/PMC10247854/
- C13 https://dev.epicgames.com/documentation/en-us/unreal-engine/subsurface-profile-shading-model-in-unreal-engine
- C14 https://dev.epicgames.com/documentation/unreal-engine/creating-human-skin-in-unreal-engine
- C15 https://theasc.com/article/shot-craft-eye-lights/
- C16 https://gamingbolt.com/silent-hill-2-remake-graphics-analysis-pushing-unreal-engine-5-to-its-limits
- C17 https://forums.unrealengine.com/t/extreme-lumen-ghosting-issue/1762156
- C18 https://gdcvault.com/play/1029020/-Dead-Space-Harnessing-the
- C19 https://gdcvault.com/play/1027254/Environment-Design-as-Visual-Storytelling
- C20 https://videogamecultures.org/wp-content/uploads/2025/12/VGC2025-online-Leitinger.pdf
- C21 https://80.lv/articles/creating-an-abandoned-environment-entirely-in-unreal-engine-5
- C22 https://en.wikipedia.org/wiki/Yellow_paint_debate
- C23 https://en.wikipedia.org/wiki/The_Exit_8
- C24 https://huggingface.co/microsoft/TRELLIS.2-4B
- C25 https://www.engadget.com/gaming/the-indie-game-awards-snatches-back-two-trophies-from-clair-obscur-over-its-use-of-generative-ai-164730842.html
- C26 https://www.gamedeveloper.com/business/valve-tweaks-and-clarifies-ai-disclosure-rules-for-steam
- C27 https://gdconf.com/article/gdc-2026-state-of-the-game-industry-reveals-impact-of-layoffs-generative-ai-and-more/
- C28 https://kotaku.com/divinity-gen-ai-larian-bg3-reddit-ama-2000658429
- C29 https://skyboxcritics.com/2025/07/14/mouthwashings-genesis-sick-jokes-and-the-thin-line-between-goofy-and-grotesque-an-interview-with-wrong-organ/
- C32 https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/117
- C33 https://www.resetera.com/threads/motion-blur-film-grain-chromatic-aberration-on-or-off.350509/ (검색 요약만)

언리얼 5 착시·최적화(U)

- U1 https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US
- U2 https://tomlooman.com/unreal-engine-5-8-performance-highlights/
- U3 https://tomlooman.com/unreal-engine-5-7-performance-highlights/
- U4 https://dev.epicgames.com/documentation/unreal-engine/nanite-virtualized-geometry-in-unreal-engine
- U5 https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine?application_version=5.6
- U6 https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine?application_version=5.6
- U7 https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine?application_version=5.6
- U9 https://dev.epicgames.com/documentation/unreal-engine/megalights-in-unreal-engine
- U10 https://dev.epicgames.com/documentation/en-us/unreal-engine/impostor-baker-plugin-in-unreal-engine
- U12 https://dev.epicgames.com/documentation/en-us/unreal-engine/using-mesh-decals-in-unreal-engine
- U13 https://dev.epicgames.com/documentation/unreal-engine/using-light-functions-in-unreal-engine
- U14 https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine
- U15 https://dev.epicgames.com/documentation/en-us/unreal-engine/local-fog-volumes-in-unreal-engine
- U16 https://dev.epicgames.com/documentation/en-us/unreal-engine/auto-exposure-in-unreal-engine
- U18 http://joostdevblog.blogspot.com/2018/09/interior-mapping-real-rooms-without.html
- U19 https://www.proun-game.com/Oogst3D/CODING/InteriorMapping/InteriorMapping.pdf
- U20 https://discussions.unity.com/threads/interior-mapping.424676/ (bgolus의 10번 글)
- U21 https://andrewgotow.com/2018/09/09/interior-mapping-part-2/
- U22 https://www.andrewwillmott.com/talks/from-aaa-to-indie-graphics-r-d
- U23 https://www.a-maze.games/blog/material-blend-decal
- U24 https://80.lv/articles/breakdown-advanced-vertex-painting-in-ue5
- U25 https://www.gamedeveloper.com/design/fog-of-woe-what-the-silent-hill-2-remake-gets-right-about-immersing-players-in-its-world

2024~2026 공포 게임 흐름(T)

- T1 https://cine21.com/news/view/?mag_id=107858
- T3 https://toneglow.substack.com/p/tone-glow-230-akira-yamaoka
- T4 https://adventuregamehotspot.com/review/1920/still-wakes-the-deep
- T5 https://noisypixel.net/no-im-not-a-human-review/
- T6 https://www.aiandgames.com/p/how-the-beast-works-in-amnesia-the
- T7 https://storymodeinfo.substack.com/p/still-wakes-the-deep-lead-designer
- T8 https://wccftech.com/review/a-quiet-place-the-road-ahead-review/
- T10 https://blog.playstation.com/2026/03/11/how-silent-hill-f-developers-crafted-tense-melee-only-combat/
- T13 https://gamemakers.jp/article/2024_02_14_60602/
- T14 https://gamemakers.jp/article/2024_07_09_72386/
- T17 https://www.asoundeffect.com/the-midnight-walk-game-audio/
- T19 https://zdnet.co.kr/view/?no=20220823134149
- T20 https://www.4gamer.net/games/751/G075133/20241203035/
- T22 https://en.wikipedia.org/wiki/2025_South_Korea_floods
- T23 https://worthplaying.com/article/2026/2/25/reviews/149105-ps5-review-resident-evil-requiem/
- T25 https://www.clien.net/service/board/park/6294430
- T26 https://tilnote.io/en/pages/6ab93680a264c4663c5c9e73
- T30 https://www.kci.go.kr/kciportal/landing/article.kci?arti_id=ART003186999
- T31 https://v.daum.net/v/20191109161002916
- T32 https://www.mt.co.kr/tech/2022/09/26/2022092615171610023
- T35 https://casenote.kr/%EB%B2%95%EC%A0%9C%EC%B2%98/21-0594-54b548
- T38 https://gdcvault.com/play/1035499/Empathy-and-Horror-Creature-Audio
- T41 https://www.kwtx.com/2025/07/30/escaping-house-your-mind-luto-review/
- T42 https://store.steampowered.com/curator/45967956-Exit-8-likes-Anomaly-Games/
- T48 https://en.wikipedia.org/wiki/Look_Outside
