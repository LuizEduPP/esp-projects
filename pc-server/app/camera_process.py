"""Pipeline de imagem — todo tratamento fica aqui (crop, resize, nitidez, compressão).

A S3 envia apenas JPEG bruto do sensor OV2640. A CYD só blit o JPEG display
já processado (320×201). Nenhum firmware faz crop, resize ou re-encode para TFT.
"""

from io import BytesIO

from PIL import Image, ImageFilter

DISPLAY_W = 320
DISPLAY_H = 201
DISPLAY_JPEG_QUALITY = 85

# None = orientação natural do sensor (vflip/hmirror desligados na S3).
# Se ainda ficar invertido, use Image.Transpose.ROTATE_180 ou FLIP_TOP_BOTTOM.
DISPLAY_ROTATE = None


def _jpeg_dimensions(raw: bytes) -> tuple[int, int]:
    for i in range(len(raw) - 9):
        if raw[i] != 0xFF:
            continue
        marker = raw[i + 1]
        if marker in (0xC0, 0xC1, 0xC2):
            h = (raw[i + 5] << 8) | raw[i + 6]
            w = (raw[i + 7] << 8) | raw[i + 8]
            return w, h
    return 0, 0


def _fit_display(img: Image.Image) -> tuple[Image.Image, bool]:
    """Crop/resize para 320×201. Retorna (imagem, heavy_resize)."""
    src_w, src_h = img.size
    heavy_resize = False

    if src_w == DISPLAY_W and src_h == DISPLAY_H:
        return img, heavy_resize

    if src_w == DISPLAY_W and src_h >= DISPLAY_H:
        top = (src_h - DISPLAY_H) // 2
        return img.crop((0, top, DISPLAY_W, top + DISPLAY_H)), heavy_resize

    target_ratio = DISPLAY_W / DISPLAY_H
    src_ratio = src_w / src_h

    if src_w >= DISPLAY_W and src_h >= DISPLAY_H:
        if src_ratio > target_ratio:
            crop_w = int(src_h * target_ratio)
            left = (src_w - crop_w) // 2
            img = img.crop((left, 0, left + crop_w, src_h))
        else:
            crop_h = int(src_w / target_ratio)
            top = (src_h - crop_h) // 2
            img = img.crop((0, top, src_w, top + crop_h))

        if img.size != (DISPLAY_W, DISPLAY_H):
            heavy_resize = True
            img = img.resize((DISPLAY_W, DISPLAY_H), Image.Resampling.BILINEAR)
        return img, heavy_resize

    heavy_resize = True
    return img.resize((DISPLAY_W, DISPLAY_H), Image.Resampling.BILINEAR), heavy_resize


def _smooth_for_display(img: Image.Image) -> Image.Image:
    """Suaviza sem exagerar nitidez — reduz ruído e banding do JPEG."""
    img = img.filter(ImageFilter.GaussianBlur(radius=0.35))
    return img.filter(ImageFilter.UnsharpMask(radius=0.5, percent=85, threshold=4))


def prepare_display_jpeg(raw: bytes) -> tuple[bytes, int, int]:
    """Gera JPEG display a partir do bruto recebido da S3."""
    raw_w, raw_h = _jpeg_dimensions(raw)

    with Image.open(BytesIO(raw)) as img:
        img = img.convert("RGB")
        if DISPLAY_ROTATE is not None:
            img = img.transpose(DISPLAY_ROTATE)
        img, _heavy_resize = _fit_display(img)
        img = _smooth_for_display(img)

        out = BytesIO()
        img.save(
            out,
            format="JPEG",
            quality=DISPLAY_JPEG_QUALITY,
            subsampling=1,
            progressive=False,
            optimize=True,
        )
        return out.getvalue(), raw_w or img.width, raw_h or img.height
