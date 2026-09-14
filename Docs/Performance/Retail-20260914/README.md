# 편의점과 뒷골목 성능 측정

2026년 9월 14일, RTX 3060 8GB, UE 5.8 Development 에디터의 게임 실행으로 측정했다.
1920×1080, D3D12, 기본 그래픽 설정이며 VSync와 프레임 제한을 껐다.
PNG 저장과 모델 생성은 실행하지 않았다. 입장 후 14초를 기다리고 계산대·진열대·냉장고·배송 골목을 순서대로 보았다.

공용 ORM 샘플러 수정 후 15:35에 측정했다. 전체 1,383프레임 중 처음 120프레임을 제외한 1,263프레임이며, 장면 전환도 표본에 포함한다.

| 항목 | 중앙값 | 95백분위 | 최대 |
| --- | ---: | ---: | ---: |
| 프레임 시간 | 21.953ms | 24.650ms | 37.778ms |
| GPU | 21.379ms | 23.631ms | 29.385ms |
| 게임 스레드 | 2.884ms | 3.718ms | 6.569ms |
| 드로 콜 | 409 | 491 | 574 |
| GPU 메모리 | 3,556.3MiB | 3,556.3MiB | 3,591.7MiB |

33.3ms를 넘긴 프레임은 1개, 100ms를 넘긴 프레임은 0개였다. 중앙값은 약 46fps이며 60fps 고정 조건은 충족하지 않는다.
GPU 조명 패스 중앙값이 7.646ms로 가장 컸고, Lumen 반사가 2.935ms였다. 이번 결과만으로 긴 플레이나 다른 장비의 성능을 보장할 수 없다.

재질 교체 직후 15:23의 첫 야간 실행에서 `DXGI_ERROR_DEVICE_HUNG`이 한 번 발생했다. 이후 같은 설정의 야간 두 차례, 편의점 캡처, 도입부 이동, 이 성능 측정은 정상 종료됐다. 재현되지 않아 원인은 확정하지 못했다. 최초 로그는 로컬 `Saved/Logs/RetailSamplerFirstLaunchGpuCrash.log`에 보존했다.

[전체 CSV 압축본](full-profile.csv.zip), [분석에 쓴 프레임](frames.csv), [요약](summary.json)을 함께 남겼다.
전체 CSV의 SHA-256은 `9a9fb5ecb9ea0dab925bfa455fbf6ec570a2f5c0e15a53b14dc1f9d0f7e9ac8d`다.

```powershell
pwsh -NoProfile -File Scripts/Run-RetailReview.ps1 -Measure
python Scripts/summarize_runtime_profile.py <새로 생긴 CSV> Docs/Performance/Retail-20260914 --warmup 120
```
