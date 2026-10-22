"""Shared encoding for shipped card faces and workshop imports."""
import io

from PIL import Image, ImageOps

ART_FORMATS = ('.png', '.jpg', '.jpeg', '.webp')
FACE_QUALITY = 94


def webp_bytes(image, *, lossless=False, quality=FACE_QUALITY):
    output = io.BytesIO()
    image.save(output, format='WEBP', quality=quality, method=6,
               lossless=lossless, exact=True)
    return output.getvalue()


def render_art(raw):
    """Retain the existing portrait dimensions and render both sizes from source."""
    with Image.open(io.BytesIO(raw)) as source:
        if getattr(source, 'n_frames', 1) != 1:
            raise ValueError('Animated images are not supported')
        image = ImageOps.exif_transpose(source).convert('RGBA')
        image.thumbnail((1200, 1200), Image.Resampling.LANCZOS)
        face = webp_bytes(image)
        image.thumbnail((320, 480), Image.Resampling.LANCZOS)
        thumb = io.BytesIO()
        image.save(thumb, format='WEBP', quality=85, method=4)
        return face, thumb.getvalue()
