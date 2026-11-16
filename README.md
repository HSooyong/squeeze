# SQUEEZE

Bearfield 교육용 컴프레서. 하나의 플러그인에서 네 가지 압축 방식(LAMP, NEEDLE, SPARK, TUBE)을 바꿔 가며 동작 원리를 본다.

M1은 빌드 골격, 파라미터, 상태 저장, 자리 표시 에디터다. M2는 네 모델의 게인 법칙과 시간 동작, 오버샘플링, 레벨 매칭, 화면용 측정값이다. 제안 계수는 `Source/Core/Tunables.h`에 있다.

식별자: 회사 Bearfield, 제조사 코드 `Brfd`, 플러그인 코드 `SqzE`, 번들 `com.bearfield.SQUEEZE`.

네트워크, 프롬프트, 자연어 입력은 넣지 않는다.

## macOS (arm64, macOS 14.7)

Xcode, CMake 3.22 이상, git이 필요하다. JUCE 8.0.15는 구성 중에 받는다.

```bash
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.7
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

포맷은 AU, VST3, Standalone이다. 맥에서 Audio Unit을 확인할 때는 `auval -v aufx SqzE Brfd`를 쓴다. 이 저장소의 CI는 그 명령을 실행하지 않는다.

## Linux

VST3와 Standalone만 만든다. 개발 패키지는 ALSA, JACK, Freetype, Fontconfig, X11이다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Windows

Visual Studio 생성기가 기본이다. VST3와 Standalone만 만든다.

```bash
cmake -S . -B build
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## 테스트

`squeeze_tests`가 ctest 이름 세 개를 제공한다.

| 이름 | 확인하는 것 |
|---|---|
| `params` | 파라미터 ID, 순서, 범위, 기본값, 로그 눈금, 표시 문자열, 창 크기 |
| `state` | 저장 후 복원, 모르는 키, 없는 키의 기본값, 이전 버전 |
| `process` | 44.1–192 kHz에서 유한한 출력, 모드를 바꿔도 지연이 같음 |
| `static_curve` | 1 kHz 정상 상태 게인이 공통 곡선과 ±0.5 dB |
| `timing_opto` | 어택 약 10 ms, 빠른 릴리즈, 긴 유지 후 느린 단이 두 배 이상 |
| `timing_vca` | 10/20/30 dB 어택, 릴리즈 120 dB/s |
| `timing_fet` | 96 kHz와 192 kHz에서 어택·릴리즈 ±15% |
| `timing_varimu` | 여섯 시간 상수 ±20%, T5의 프로그램 의존 |
| `null_bypass` | 게인 0에서 지연 보정 후 오차 ≤ −100 dBFS |
| `level_match` | 서로 다른 두 설정의 라우드니스 차 ≤ 0.5 LU |
| `stability` | 샘플 레이트·블록 크기, 60초 무작위 조작, 무음 후 비정상 부동소수 없음 |
| `mode_switch` | 전환 중 지연 불변, 샘플 점프 없음 |
| `visual_contract` | FET 어택 측정 샘플이 설정과 ±1 (192 kHz) |
| `perf` | 모드별 ns/sample 보고 |

한 가지만 돌릴 때는 `./build/squeeze_tests params`처럼 이름을 넘긴다. 다중 구성 생성기에서는 Release 산출물 경로를 쓴다.
