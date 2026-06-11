from dataclasses import dataclass, field
from datetime import datetime, timezone
from threading import Lock

from app.camera_process import prepare_display_jpeg

_lock = Lock()
_frames: dict[str, "CameraFrame"] = {}


@dataclass
class CameraFrame:
    device_id: str
    frame_id: int
    raw_size: int
    raw_w: int
    raw_h: int
    raw_data: bytes
    display_size: int
    display_data: bytes
    updated_at: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())


def save_frame(device_id: str, frame_id: int, raw_data: bytes) -> CameraFrame:
    display_data, raw_w, raw_h = prepare_display_jpeg(raw_data)
    frame = CameraFrame(
        device_id=device_id,
        frame_id=frame_id,
        raw_size=len(raw_data),
        raw_w=raw_w,
        raw_h=raw_h,
        raw_data=raw_data,
        display_size=len(display_data),
        display_data=display_data,
    )
    with _lock:
        current = _frames.get(device_id)
        if current is not None and frame_id <= current.frame_id:
            frame.frame_id = current.frame_id + 1
        _frames[device_id] = frame
    return frame


def frame_info(device_id: str, since: int = 0) -> dict | None:
    with _lock:
        frame = _frames.get(device_id)
        if frame is None:
            return None
        return {
            "device_id": frame.device_id,
            "frame_id": frame.frame_id,
            "raw_size": frame.raw_size,
            "raw_w": frame.raw_w,
            "raw_h": frame.raw_h,
            "display_size": frame.display_size,
            "updated_at": frame.updated_at,
            "changed": frame.frame_id > since,
        }


def latest_display(device_id: str, since: int = 0) -> CameraFrame | None:
    with _lock:
        frame = _frames.get(device_id)
        if frame is None or frame.frame_id <= since:
            return None
        return frame
