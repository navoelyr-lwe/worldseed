# Fizibilite — OpenCL 1.2 / OpenCL C 1.2 ve CPU fallback

**Aşama bağı:** P10 (P0/P2 spike). **Karar kaydı:** ADR-001.

## Amaç

OpenCL 1.2 API ve OpenCL C 1.2 kernel yolunun, sistem sürücüsü üzerinden çalıştırılabildiğini ve GPU yokluğunda CPU fallback'in dünyayı açmayı engellemediğini kanıtlamak.

## Kapsam (spike)

1. `OpenCL.dll` / `libOpenCL.so.1` sistem loader üzerinden dinamik yükleme.
2. Platform/cihaz keşfi, cihaz adı/sürümü, compute unit ve global bellek sorgusu.
3. Cihaz yokluğunda hata değil, CPU fallback kararı.
4. Hiçbir OpenCL runtime/driver paketinin ürünle dağıtılmaması.

## Uygulanan prototip (bu PR)

- `src/platform/ws_dynlib.{h,c}`: platform dinamik kütüphane adaptörü.
- `src/platform/ws_opencl_probe.{h,c}`: OpenCL 1.2 alt kümesiyle yetenek sorgusu.
- ISO C `-Wpedantic` uyumu için obje→fonksiyon işaretçisi dönüşümü union üzerinden yapılır (platform-özel, belgelenmiş).

## Kanıt

- CI'da `ws_smoke` OpenCL satırı: `opencl loaded=0|1 device_found=0|1 ...`.
- GPU'suz GitHub-hosted runner'da `device_found=0` ve `WS_SMOKE_OK` — fallback yolu kanıtlanır.

## Sınırlar ve sonraki iş

- Bu spike kernel derleme/çalıştırma içermez; yalnız yetenek keşfi yapar.
- Gerçek GPU'da OpenCL C 1.2 kernel derleme ve CPU/GPU semantik karşılaştırması P10 görevidir.
- Kernel kaynakları `.cl` olarak ayrı tutulur ve yalnız OpenCL C 1.2 alt kümesini kullanır.
- GPU sonucu otoritatif simülasyonu etkilemez (ADR-006); GPU öncelikle render/görsel compute'u hızlandırır.

## Karar

OpenCL 1.2 keşif yolu ve CPU fallback uygulanabilir ve bağımlılık eklemez. GPU kernel geliştirmesi P10'a bırakılır; bu spike yeni bir GPU dili/API seçmez.
