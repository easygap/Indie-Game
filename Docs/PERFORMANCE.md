# Provisional Performance Budget

최소/권장 PC 사양이 확정되기 전까지 Windows PC, 1080p High, 60fps를 임시 대표 환경으로 사용합니다.

## 프레임 예산

- 전체 프레임: 16.67ms 이내
- GPU: 대표 장면에서 14ms 이내 목표
- Game Thread: 8ms 이내 목표
- Render Thread: 8ms 이내 목표
- 진행을 방해하는 50ms 이상의 긴 hitch 없음

CPU와 Render Thread는 병렬로 실행되므로 각 수치를 단순 합산하지 않습니다.

## 런타임 규칙

- Actor Tick은 기본 비활성화하고 이벤트 또는 제한 주기 타이머를 사용합니다.
- 플레이 중 동기 에셋 로드는 금지하며 챕터 데이터의 soft reference를 미리 비동기 로드합니다.
- 상호작용 trace는 플레이어당 하나, 10~15Hz를 기본값으로 합니다.
- 공간 오디오는 virtualization과 streaming을 사용하고 동시 활성 음성을 관리합니다.
- 일반 소품 텍스처는 1K, hero 소품은 2K를 출발점으로 하며 4K는 화면 점유 근거가 있을 때만 사용합니다.
- Lumen을 사용할 때 shadow-casting movable light의 수와 영향 반경을 장면별로 제한합니다.
- 정적 고밀도 환경 메시에는 Nanite를 검토하되 단순 collision을 별도로 제공합니다.

## 회귀 점검

각 마일스톤에서 대표 체크포인트를 동일하게 재생하며 다음을 기록합니다.

- `stat unit`, `stat gpu`
- Unreal Insights CPU, loading과 hitch trace
- `MemReport`
- Size Map과 Reference Viewer
- Development 및 Shipping 패키지의 첫 실행/재실행 차이

정확한 VRAM, 시스템 메모리, 설치 용량과 scalability tier별 예산은 목표 GPU가 확정된 뒤 고정합니다.
