# SQUEEZE

Bearfield 교육용 컴프레서. 하나의 플러그인에서 네 가지 압축 방식(LAMP, NEEDLE, SPARK, TUBE)을 바꿔 가며 동작 원리를 본다.

M1은 빌드 골격, 파라미터, 상태 저장, 자리 표시 에디터, 단위 테스트까지다. 오디오 처리는 통과(pass-through)다. 모델 계산, 오버샘플링, 레벨 매칭은 다음 마일스톤이다.

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
| `params` | 파라미터 ID, 순서, 범위, 기본값, 로그 눈금, 표시 문자열, 창 크기 상수, 버스 레이아웃 |
| `state` | 저장 후 복원, 모르는 키 무시, 없는 키는 기본값, 버전 0과 버전 없는 문서, History 50개 제한 |
| `process` | 44.1–192 kHz, 모드 전환, 바이패스, 지연 0 유지, NaN 없음, 통과 처리 |

한 가지만 돌릴 때는 `./build/squeeze_tests params`처럼 이름을 넘긴다. 다중 구성 생성기에서는 Release 산출물 경로를 쓴다.
