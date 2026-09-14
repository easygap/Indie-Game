# 편의점과 뒷골목 성능 측정

2026년 9월 14일, RTX 3060 8GB, UE 5.8 Development 에디터의 게임 실행으로 측정했다.
1920×1080, D3D12, 기본 그래픽 설정이며 VSync와 프레임 제한을 껐다.
PNG 저장과 모델 생성은 실행하지 않았다. 입장 후 14초를 기다리고 계산대·진열대·냉장고·배송 골목을 순서대로 보았다.

공용 ORM 샘플러와 금속색 베이크를 고치고 최종 에셋 66개를 반입한 뒤 16:50에 측정했다. 전체 1,442프레임 중 처음 120프레임을 제외한 1,322프레임이며, 장면 전환도 표본에 포함한다.

| 항목 | 중앙값 | 95백분위 | 최대 |
| --- | ---: | ---: | ---: |
| 프레임 시간 | 21.046ms | 23.447ms | 30.528ms |
| GPU | 20.754ms | 22.141ms | 27.635ms |
| 게임 스레드 | 2.923ms | 3.702ms | 6.529ms |
| 드로 콜 | 409 | 491 | 576 |
| GPU 메모리 | 3,561.3MiB | 3,562.0MiB | 3,597.4MiB |

33.3ms를 넘긴 프레임과 100ms를 넘긴 프레임은 모두 0개였다. 중앙값은 약 48fps이며 60fps 고정 조건은 충족하지 않는다.
이번 결과는 위 네 시점의 짧은 측정이다. 긴 플레이와 다른 장비의 성능은 별도 검증이 필요하다.

재질 교체 직후 15:23의 첫 야간 실행에서 `DXGI_ERROR_DEVICE_HUNG`이 한 번 발생했다. 그 뒤 같은 설정으로 반복한 야간·편의점·도입부 실행과 최종 성능 측정은 정상 종료됐다. 재현되지 않아 원인은 확정하지 못했다. 최초 로그는 로컬 `Saved/Logs/RetailSamplerFirstLaunchGpuCrash.log`에 보존했다.

[전체 CSV 압축본](full-profile.csv.zip), [분석에 쓴 프레임](frames.csv), [요약](summary.json)을 함께 남겼다.
전체 CSV의 SHA-256은 `6e8e3dea2e270da20a950f69a538fadf519b255a59520e148ed7f5f3036e48fe`다.

```powershell
pwsh -NoProfile -File Scripts/Run-RetailReview.ps1 -Measure
python Scripts/summarize_runtime_profile.py <새로 생긴 CSV> Docs/Performance/Retail-20260914 --warmup 120
```
