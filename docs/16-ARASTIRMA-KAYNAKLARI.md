# 16 — Araştırma başlangıç kaynakları

Bu liste ilk araştırma noktalarıdır; tek başına model doğrulaması sayılmaz. OpenHands her model için güncel makale, deneysel veri, lisans ve geçerlilik aralığını ayrıca araştırmalıdır. Kaynaklar kopyalanacak kod/dependency olarak değil yöntemsel referans olarak kullanılır.

## Dünya sistemi ve çok ölçekli modelleme

- U.S. DOE — Earth System Model Development: https://eesm.science.energy.gov/program-area/earth-system-model-development
- NASA GISS ModelE: https://www.giss.nasa.gov/tools/modelE/
- WCRP CMIP Overview: https://wcrp-cmip.org/cmip-overview/
- U.S. DOE — Earth System and Climate Models: https://www.energy.gov/science/doe-explainsearth-system-and-climate-models

## Biyomekanik

- OpenSim, Stanford: https://opensim.stanford.edu/
- OpenSim documentation: https://opensimconfluence.atlassian.net/wiki/spaces/OpenSim/pages/53088473/Documentation
- OpenSim publication/resource portal: https://simtk.org/projects/opensim

OpenSim burada yöntemsel ve bilimsel referanstır; ürün bağımlılığı veya kod kopyalama izni değildir.

## GPU ve C tabanlı programlama

- Khronos OpenCL Registry: https://registry.khronos.org/OpenCL/
- OpenCL C specification: https://registry.khronos.org/OpenCL/specs/unified/html/OpenCL_C.html
- OpenCL API specification: https://registry.khronos.org/OpenCL/specs/unified/html/OpenCL_API.html
- Khronos OpenCL 3.0 compatibility overview: https://github.com/KhronosGroup/OpenCL-Guide/blob/main/chapters/opencl_3.md
- NVIDIA OpenCL support: https://developer.nvidia.com/opencl
- Khronos conformant OpenCL products: https://www.khronos.org/conformance/adopters/conformant-products/opencl

Karar sabittir: OpenCL 1.2 API ve OpenCL C 1.2 kernel alt kümesi. OpenCL C, C99 tabanlı ayrı bir C lehçesidir; ISO C17 değildir. OpenCL 3.x cihazlarda 1.2 uyumluluk tabanını hedefler, opsiyonel 2.x/3.x özellikleri kullanmaz. Bu, C-only motor için seçilmiş tek GPU C-lehçesi istisnasıdır.

## Bilimsel model geliştirme ilkeleri

Araştırma sırasında ayrıca şu konu grupları incelenir:

- Sayısal çözümleyiciler ve kararlılık.
- Katı cisim dinamiği ve temas.
- Hayvan lokomosyonu ve kas-iskelet biyomekaniği.
- Termoregülasyon ve enerji bütçesi.
- Bitki büyümesi, su ve ışık bütçesi.
- Besin ağı ve popülasyon ekolojisi.
- Akışkanlar ve çevresel süreçler.
- Görüntüleme: PBR, HDR, gölge, atmosfer, su, vegetation LOD.
- Doğal ses ve mekânsal akustik.

Her alt konu için OpenHands kaynak künyesi, DOI/kalıcı bağlantı, erişim tarihi, veri lisansı ve model kartı üretmelidir.
