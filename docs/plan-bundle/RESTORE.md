# Plan paketini geri açma

Bu klasördeki `part-01.txt` … `part-16.txt` dosyaları, tam ürün planının aktarım için Base64'e bölünmüş parçalarıdır. Sıraları dosya adındaki numaraya göredir.

## Beklenen bütünlük

Birleştirilip Base64 çözüldükten sonra elde edilen 34,198 baytlık aktarım dosyasının SHA-256 değeri:

`69c49e5d7868fe1d83c2559f5da703a95eb443d18e4a5bd8b29eaa6c5a8f2f5a`

Aktarım dosyası iki kat gzip içerir; önce dış, sonra iç gzip açılır. İçerik GNU tar arşividir. Bu iki katman yalnızca bu belge paketinin GitHub'a güvenilir aktarımı içindir; ürün mimarisiyle ilgili değildir.

## Geri açma

Depo kökünde Python 3 standart kütüphanesiyle aşağıdaki işlemi çalıştır. Bu yalnızca ilk dokümantasyon kurulumudur; ürünün, motorun veya dağıtılan araçların Python bağımlılığı değildir.

```python
from pathlib import Path
import base64
import gzip
import hashlib
import io
import tarfile

bundle_dir = Path("docs/plan-bundle")
parts = sorted(bundle_dir.glob("part-*.txt"))
assert len(parts) == 16, f"16 parça bekleniyor, bulunan: {len(parts)}"

encoded = "".join(p.read_text(encoding="ascii").strip() for p in parts)
payload = base64.b64decode(encoded, validate=True)
expected = "69c49e5d7868fe1d83c2559f5da703a95eb443d18e4a5bd8b29eaa6c5a8f2f5a"
actual = hashlib.sha256(payload).hexdigest()
assert actual == expected, f"Arşiv özeti uyuşmuyor: {actual}"

tar_layer = gzip.decompress(gzip.decompress(payload))
destination = Path("docs")
destination.mkdir(exist_ok=True)

with tarfile.open(fileobj=io.BytesIO(tar_layer), mode="r:") as archive:
    members = archive.getmembers()
    for member in members:
        path = Path(member.name)
        assert not path.is_absolute() and ".." not in path.parts, member.name
        assert member.isfile() or member.isdir(), f"Bağlantı/özel dosya reddedildi: {member.name}"

    source_root = "worldseed-plan-v0.1/"
    for member in members:
        if not member.isfile():
            continue
        relative = member.name.removeprefix(source_root)
        if not relative:
            continue
        if relative == "README.md":
            target = destination / "PLAN-README.md"
        elif relative == "AGENTS.md":
            target = destination / "PLAN-AGENTS-REFERENCE.md"
        elif relative == "PLAN-MANIFEST.json":
            target = destination / "PLAN-MANIFEST.json"
        else:
            target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        source = archive.extractfile(member)
        assert source is not None
        target.write_bytes(source.read())

print(f"Plan geri açıldı: {len([m for m in members if m.isfile()])} dosya; SHA-256 doğrulandı.")
```

Geri açıldıktan sonra `docs/00-URUN-SOZLESMESI.md` ana bağlayıcı ürün belgesi, `docs/13-YOL-HARITASI.md` aşama sırası ve `docs/14-OPENHANDS-CALISMA-KURALLARI.md` ajan çalışma politikasıdır. Root `AGENTS.md` bunlarla tutarlı kalmalıdır.

OpenHands ilk görevinde bu geri açmayı yapmalı, dosyaları README'de bağlantılandırmalı, manifest ve dosya listesini doğrulamalı, çelişki/eksik taraması yapmalı ve tek bir dokümantasyon/altyapı PR'ı açmalıdır. İlk görevde ürün özelliği kodlamaya başlamamalıdır.
