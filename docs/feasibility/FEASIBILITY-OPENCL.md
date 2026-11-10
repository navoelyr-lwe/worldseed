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

## P10 sonucu (tamamlandı)

P10 bu spike'ı tamamladı:

- `kernels/ws_raster.cl` OpenCL C 1.2 kernel'i derleme zamanında gömülür ve
  `-cl-std=CL1.2` ile derlenir (`src/gpu/ws_opencl.*`).
- CPU referans rasterleyicisi (`src/render/ws_raster.c`) ve kernel aynı deterministik
  yığın üzerinde koşar; semantik karşılaştırma tolerans içinde geçer
  (`tools/ws_gpu_check`: `diff_pixels=1/49152`, `max_channel_delta=1`).
- Device-loss kurtarma ve CPU fallback doğrulanır (`device_loss_recovered=1`).
- CI `opencl-cpu` işi pocl CPU ICD ile gerçek kernel derleme/çalıştırma kanıtı üretir.
- Kanıtlar: `docs/P10-GPU-VE-PERFORMANS.md`, `docs/gpu/MODEL-KARTI-GPU-RASTER.md`.

Gerçek GPU donanımı bu ortamda yoktur; hızlanma iddiası yalnız referans donanımda
ölçümle yapılır. Detay `docs/P10-GPU-VE-PERFORMANS.md` "Bilinen sınırlamalar".

- Kernel kaynakları `.cl` olarak ayrı tutulur ve yalnız OpenCL C 1.2 alt kümesini kullanır.
- GPU sonucu otoritatif simülasyonu etkilemez (ADR-001); GPU öncelikle render/görsel compute'u hızlandırır.

## Karar

OpenCL 1.2 keşif yolu ve CPU fallback uygulanabilir ve bağımlılık eklemez. GPU kernel geliştirmesi P10'da tamamlandı (bkz. yukarıdaki "P10 sonucu"); bu yol yeni bir GPU dili/API seçmez.
