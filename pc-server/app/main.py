from datetime import datetime, timezone
from typing import Any

from fastapi import FastAPI, HTTPException, Request, Response
from pydantic import BaseModel

from app.camera_store import frame_info, latest_display, save_frame
from app.firmware_store import firmware_binary_path, firmware_info, load_manifest

app = FastAPI(title="edge-pair pc-server", version="0.2.0")

_health_log: list[dict[str, Any]] = []


class HealthBody(BaseModel):
    device_id: str
    role: str
    uptime_sec: int = 0
    pair_ready: bool = False
    firmware_version: str = "0.0.0"


@app.get("/")
def root() -> dict[str, str]:
    return {"service": "edge-pair", "docs": "/docs"}


def _base_url(request: Request) -> str:
    return str(request.base_url).rstrip("/")


@app.post("/api/v1/health")
def post_health(body: HealthBody, request: Request) -> dict[str, Any]:
    entry = {
        "device_id": body.device_id,
        "role": body.role,
        "uptime_sec": body.uptime_sec,
        "pair_ready": body.pair_ready,
        "firmware_version": body.firmware_version,
        "ts": datetime.now(timezone.utc).isoformat(),
    }
    _health_log.append(entry)
    if len(_health_log) > 100:
        _health_log.pop(0)

    update = firmware_info(body.role, body.firmware_version, _base_url(request))
    response: dict[str, Any] = {"ok": True, **entry}
    if update:
        response["firmware_update"] = update
    return response


@app.get("/api/v1/health/recent")
def recent_health() -> dict[str, Any]:
    return {"count": len(_health_log), "entries": _health_log[-20:]}


@app.get("/api/v1/firmware/manifest")
def get_firmware_manifest() -> dict[str, Any]:
    return load_manifest()


@app.get("/api/v1/firmware/check")
def check_firmware(role: str, version: str, request: Request) -> dict[str, Any]:
    if role not in ("cyd", "s3"):
        raise HTTPException(status_code=400, detail="role must be cyd or s3")
    update = firmware_info(role, version, _base_url(request))
    if update:
        return update
    return {"update": False, "version": version}


@app.get("/api/v1/firmware/{role}/firmware.bin")
def download_firmware(role: str) -> Response:
    if role not in ("cyd", "s3"):
        raise HTTPException(status_code=400, detail="role must be cyd or s3")
    path = firmware_binary_path(role)
    if path is None:
        raise HTTPException(status_code=404, detail="firmware not found")
    data = path.read_bytes()
    return Response(
        content=data,
        media_type="application/octet-stream",
        headers={"Content-Disposition": 'attachment; filename="firmware.bin"'},
    )


@app.get("/api/v1/config")
def get_config(device_id: str = "s3-cam-01") -> dict[str, Any]:
    return {
        "device_id": device_id,
        "sens": 80,
        "fps": 5,
        "motion_enabled": True,
    }


@app.get("/api/v1/ui/state")
def get_ui_state(device_id: str = "cyd-01") -> dict[str, Any]:
    cam = frame_info("s3-cam-01")
    lines = ["Stream via pc-server", "UART: pareamento"]
    if cam:
        lines = [
            f"#{cam['frame_id']} raw {cam['raw_w']}x{cam['raw_h']} disp {cam['display_size']}B",
            "Stream Wi-Fi ativo",
        ]
    return {
        "device_id": device_id,
        "screen": "stream" if cam else "idle",
        "title": "Edge Pair",
        "lines": lines,
        "alert": None,
    }


@app.get("/api/v1/camera/frame/info")
def get_camera_frame_info(device_id: str = "s3-cam-01", since: int = 0) -> dict[str, Any]:
    info = frame_info(device_id, since=since)
    if info is None:
        return {
            "device_id": device_id,
            "frame_id": 0,
            "raw_size": 0,
            "display_size": 0,
            "updated_at": None,
            "changed": False,
        }
    return info


@app.get("/api/v1/camera/frame/display")
def get_camera_frame_display(device_id: str = "s3-cam-01", since: int = 0) -> Response:
    frame = latest_display(device_id, since=since)
    if frame is None:
        raise HTTPException(status_code=404, detail="no new frame")
    return Response(
        content=frame.display_data,
        media_type="image/jpeg",
        headers={
            "X-Frame-Id": str(frame.frame_id),
            "X-Frame-Size": str(frame.display_size),
            "X-Display-Width": "320",
            "X-Display-Height": "201",
            "Cache-Control": "no-store",
        },
    )


@app.get("/api/v1/camera/frame/latest")
def get_camera_frame_latest(device_id: str = "s3-cam-01", since: int = 0) -> Response:
    return get_camera_frame_display(device_id, since)


@app.post("/api/v1/camera/frame")
async def post_camera_frame(
    request: Request,
    device_id: str = "s3-cam-01",
    frame_id: int = 0,
) -> dict[str, Any]:
    """Recebe JPEG bruto da S3; crop/resize/nitidez/compressão display só aqui."""
    data = await request.body()
    if len(data) < 512:
        raise HTTPException(status_code=400, detail="jpeg too small")
    if data[0] != 0xFF or data[1] != 0xD8:
        raise HTTPException(status_code=400, detail="invalid jpeg")
    saved = save_frame(device_id, frame_id, data)
    return {
        "ok": True,
        "device_id": saved.device_id,
        "frame_id": saved.frame_id,
        "raw_size": saved.raw_size,
        "display_size": saved.display_size,
        "updated_at": saved.updated_at,
    }
