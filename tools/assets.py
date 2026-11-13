#!/usr/bin/env python3
"""Generate stylised B/W school GLB assets via Tripo3D + Hyper3D Rodin (stdlib only)."""

from __future__ import annotations

import json
import os
import sys
import threading
import time
import uuid
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Any, Callable
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

REPO_ROOT = Path(__file__).resolve().parent.parent
ENV_PATH = REPO_ROOT / ".env"
OUT_DIR = REPO_ROOT / "Assets" / "Generated"

STYLE = (
    "stylised black-and-white school game asset, low-poly, game-ready, "
    "clean topology, plain white and grey materials only, no text, no logos, "
    "no colour accents, simple silhouette"
)

# id -> (provider, prompt)
ASSETS: dict[str, tuple[str, str]] = {
    "desk": (
        "tripo",
        f"school classroom wooden desk, {STYLE}",
    ),
    "chair": (
        "tripo",
        f"school classroom chair, {STYLE}",
    ),
    "locker_row": (
        "tripo",
        f"row of school lockers, {STYLE}",
    ),
    "chalkboard": (
        "tripo",
        f"classroom chalkboard on a stand, blank board, {STYLE}",
    ),
    "podium": (
        "rodin",
        f"teacher podium lectern for a classroom, {STYLE}",
    ),
    "bench": (
        "tripo",
        f"outdoor school courtyard bench, {STYLE}",
    ),
    "tree_stylised": (
        "tripo",
        f"stylised school courtyard tree, {STYLE}",
    ),
    "vending_machine": (
        "rodin",
        f"school hallway vending machine, blank panels, {STYLE}",
    ),
    "cafeteria_table": (
        "tripo",
        f"school cafeteria table with attached seats, {STYLE}",
    ),
    "school_gate": (
        "rodin",
        f"school entrance gate, {STYLE}",
    ),
    "bookshelf": (
        "rodin",
        f"classroom bookshelf with plain book blocks, {STYLE}",
    ),
    "potted_plant": (
        "tripo",
        f"potted plant for a school hallway, {STYLE}",
    ),
    "student_male_stylised": (
        "tripo",
        "low-poly stylised high-school student male in blazer uniform, T-pose, "
        f"{STYLE}",
    ),
    "student_female_stylised": (
        "tripo",
        "low-poly stylised high-school student female in blazer and skirt uniform, "
        f"T-pose, {STYLE}",
    ),
    "teacher_stylised": (
        "tripo",
        f"low-poly stylised high-school teacher in suit, T-pose, {STYLE}",
    ),
}

TRIPO_BASE = "https://api.tripo3d.ai/v2/openapi"
RODIN_BASE = "https://hyperhuman.deemos.com/api/v2"

POLL_INTERVAL_S = 8
POLL_TIMEOUT_S = 60 * 20
HTTP_TIMEOUT_S = 120

_log_lock = threading.Lock()


def log(msg: str) -> None:
    with _log_lock:
        print(msg, flush=True)


def load_env(path: Path) -> dict[str, str]:
    env: dict[str, str] = {}
    if not path.is_file():
        raise SystemExit(f"missing .env at {path}")
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, val = line.partition("=")
        key = key.strip()
        val = val.strip().strip('"').strip("'")
        if key:
            env[key] = val
    return env


def http_json(
    method: str,
    url: str,
    *,
    headers: dict[str, str] | None = None,
    body: bytes | None = None,
) -> Any:
    hdrs = dict(headers or {})
    req = Request(url, data=body, headers=hdrs, method=method)
    try:
        with urlopen(req, timeout=HTTP_TIMEOUT_S) as resp:
            raw = resp.read()
    except HTTPError as e:
        err_body = e.read().decode("utf-8", errors="replace")[:500]
        raise RuntimeError(f"HTTP {e.code} {url}: {err_body}") from e
    except URLError as e:
        raise RuntimeError(f"URL error {url}: {e}") from e
    if not raw:
        return None
    return json.loads(raw.decode("utf-8"))


def http_download(url: str, dest: Path) -> None:
    req = Request(url, method="GET")
    try:
        with urlopen(req, timeout=HTTP_TIMEOUT_S) as resp:
            data = resp.read()
    except HTTPError as e:
        err_body = e.read().decode("utf-8", errors="replace")[:300]
        raise RuntimeError(f"download HTTP {e.code}: {err_body}") from e
    tmp = dest.with_suffix(dest.suffix + ".part")
    tmp.write_bytes(data)
    tmp.replace(dest)


def multipart_encode(fields: dict[str, str]) -> tuple[bytes, str]:
    boundary = f"----RodinBoundary{uuid.uuid4().hex}"
    lines: list[bytes] = []
    for name, value in fields.items():
        lines.append(f"--{boundary}".encode())
        lines.append(
            f'Content-Disposition: form-data; name="{name}"'.encode()
        )
        lines.append(b"")
        lines.append(value.encode("utf-8"))
    lines.append(f"--{boundary}--".encode())
    lines.append(b"")
    body = b"\r\n".join(lines)
    content_type = f"multipart/form-data; boundary={boundary}"
    return body, content_type


# --- Tripo3D ---------------------------------------------------------------

def tripo_submit(api_key: str, prompt: str) -> str:
    payload = json.dumps({"type": "text_to_model", "prompt": prompt}).encode()
    data = http_json(
        "POST",
        f"{TRIPO_BASE}/task",
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        },
        body=payload,
    )
    if not isinstance(data, dict) or data.get("code") not in (0, None):
        raise RuntimeError(f"tripo submit failed: {data!r}"[:400])
    task_id = (data.get("data") or {}).get("task_id")
    if not task_id:
        raise RuntimeError(f"tripo missing task_id: {data!r}"[:400])
    return str(task_id)


def tripo_poll(api_key: str, task_id: str) -> dict[str, Any]:
    deadline = time.time() + POLL_TIMEOUT_S
    while time.time() < deadline:
        data = http_json(
            "GET",
            f"{TRIPO_BASE}/task/{task_id}",
            headers={"Authorization": f"Bearer {api_key}"},
        )
        if not isinstance(data, dict):
            raise RuntimeError(f"tripo poll bad response: {data!r}"[:400])
        inner = data.get("data") or {}
        status = str(inner.get("status", "")).lower()
        if status == "success":
            return inner
        if status in ("failed", "cancelled", "banned", "expired"):
            raise RuntimeError(f"tripo task {task_id} status={status}: {inner!r}"[:400])
        time.sleep(POLL_INTERVAL_S)
    raise RuntimeError(f"tripo task {task_id} timed out")


def tripo_download_model(task: dict[str, Any], dest: Path) -> None:
    output = task.get("output") or {}
    url = output.get("pbr_model") or output.get("model") or output.get("base_model")
    if not url:
        raise RuntimeError(f"tripo no model url in output: {list(output.keys())}")
    http_download(str(url), dest)


def generate_tripo(api_key: str, asset_id: str, prompt: str, dest: Path) -> None:
    log(f"[{asset_id}] tripo submit")
    task_id = tripo_submit(api_key, prompt)
    log(f"[{asset_id}] tripo poll task={task_id}")
    task = tripo_poll(api_key, task_id)
    log(f"[{asset_id}] tripo download -> {dest.name}")
    tripo_download_model(task, dest)


# --- Hyper3D Rodin ---------------------------------------------------------

def rodin_submit(api_key: str, prompt: str) -> tuple[str, str]:
    """Returns (task_uuid, subscription_key)."""
    fields = {
        "prompt": prompt,
        "geometry_file_format": "glb",
        "material": "PBR",
        "quality": "medium",
        "tier": "Sketch",
    }
    body, content_type = multipart_encode(fields)
    data = http_json(
        "POST",
        f"{RODIN_BASE}/rodin",
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": content_type,
        },
        body=body,
    )
    if not isinstance(data, dict):
        raise RuntimeError(f"rodin submit bad response: {data!r}"[:400])
    if data.get("error"):
        raise RuntimeError(f"rodin submit error: {data.get('error')!r}"[:400])

    task_uuid = data.get("uuid") or data.get("task_uuid")
    jobs = data.get("jobs") or {}
    sub_key = jobs.get("subscription_key") or data.get("subscription_key")
    job_uuids = jobs.get("uuids") or []
    if not task_uuid and job_uuids:
        task_uuid = job_uuids[0]
    if not task_uuid:
        raise RuntimeError(f"rodin missing uuid: {data!r}"[:400])
    if not sub_key:
        # fall back to polling with task uuid
        sub_key = str(task_uuid)
    return str(task_uuid), str(sub_key)


def rodin_poll(api_key: str, subscription_key: str) -> None:
    deadline = time.time() + POLL_TIMEOUT_S
    payload = json.dumps({"subscription_key": subscription_key}).encode()
    # some deployments accept {"uuid": subscription_key}
    alt_payload = json.dumps({"uuid": subscription_key}).encode()
    use_alt = False
    while time.time() < deadline:
        body = alt_payload if use_alt else payload
        try:
            data = http_json(
                "POST",
                f"{RODIN_BASE}/status",
                headers={
                    "Authorization": f"Bearer {api_key}",
                    "Content-Type": "application/json",
                },
                body=body,
            )
        except RuntimeError as e:
            if not use_alt and "HTTP 4" in str(e):
                use_alt = True
                continue
            raise
        if not isinstance(data, dict):
            raise RuntimeError(f"rodin status bad response: {data!r}"[:400])
        if data.get("error"):
            # try alternate key shape once
            if not use_alt:
                use_alt = True
                continue
            raise RuntimeError(f"rodin status error: {data.get('error')!r}"[:400])

        jobs = data.get("jobs")
        statuses: list[str] = []
        if isinstance(jobs, list):
            statuses = [str(j.get("status", "")) for j in jobs]
        elif isinstance(jobs, dict):
            statuses = [str(v.get("status", "")) for v in jobs.values() if isinstance(v, dict)]
        elif data.get("status"):
            statuses = [str(data.get("status"))]

        norm = [s.lower() for s in statuses]
        if not norm:
            time.sleep(POLL_INTERVAL_S)
            continue
        if any(s in ("failed", "error", "canceled", "cancelled") for s in norm):
            raise RuntimeError(f"rodin job failed: {statuses}")
        if all(s in ("done", "completed", "success", "finished") for s in norm):
            return
        time.sleep(POLL_INTERVAL_S)
    raise RuntimeError(f"rodin poll timed out for {subscription_key}")


def rodin_download(api_key: str, task_uuid: str, dest: Path) -> None:
    payload = json.dumps({"task_uuid": task_uuid}).encode()
    data = http_json(
        "POST",
        f"{RODIN_BASE}/download",
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        },
        body=payload,
    )
    if not isinstance(data, dict):
        raise RuntimeError(f"rodin download bad response: {data!r}"[:400])
    if data.get("error"):
        raise RuntimeError(f"rodin download error: {data.get('error')!r}"[:400])

    items = data.get("list") or data.get("files") or []
    url = None
    if isinstance(items, list):
        for item in items:
            if not isinstance(item, dict):
                continue
            name = str(item.get("name") or item.get("filename") or "").lower()
            u = item.get("url") or item.get("download_url")
            if not u:
                continue
            if name.endswith(".glb") or "glb" in name or not url:
                url = u
                if name.endswith(".glb"):
                    break
    if not url and isinstance(data.get("url"), str):
        url = data["url"]
    if not url:
        raise RuntimeError(f"rodin no glb url: {data!r}"[:400])
    http_download(str(url), dest)


def generate_rodin(api_key: str, asset_id: str, prompt: str, dest: Path) -> None:
    log(f"[{asset_id}] rodin submit")
    task_uuid, sub_key = rodin_submit(api_key, prompt)
    log(f"[{asset_id}] rodin poll uuid={task_uuid}")
    rodin_poll(api_key, sub_key)
    log(f"[{asset_id}] rodin download -> {dest.name}")
    rodin_download(api_key, task_uuid, dest)


# --- runner ----------------------------------------------------------------

def run_one(
    asset_id: str,
    provider: str,
    prompt: str,
    tripo_key: str,
    rodin_key: str,
) -> tuple[str, str]:
    dest = OUT_DIR / f"{asset_id}.glb"
    if dest.is_file() and dest.stat().st_size > 0:
        log(f"[{asset_id}] skip (exists)")
        return asset_id, "skipped"

    def attempt() -> None:
        if provider == "tripo":
            generate_tripo(tripo_key, asset_id, prompt, dest)
        elif provider == "rodin":
            generate_rodin(rodin_key, asset_id, prompt, dest)
        else:
            raise RuntimeError(f"unknown provider {provider}")

    try:
        attempt()
        if not dest.is_file() or dest.stat().st_size == 0:
            raise RuntimeError("empty output file")
        log(f"[{asset_id}] OK ({dest.stat().st_size} bytes)")
        return asset_id, "ok"
    except Exception as e1:
        log(f"[{asset_id}] fail: {e1}; retrying once")
        try:
            if dest.is_file():
                dest.unlink()
        except OSError:
            pass
        try:
            attempt()
            if not dest.is_file() or dest.stat().st_size == 0:
                raise RuntimeError("empty output file")
            log(f"[{asset_id}] OK after retry ({dest.stat().st_size} bytes)")
            return asset_id, "ok"
        except Exception as e2:
            log(f"[{asset_id}] FAILED: {e2}")
            return asset_id, f"failed: {e2}"


def main() -> int:
    env = load_env(ENV_PATH)
    tripo_key = env.get("TRIPO_API_KEY", "").strip()
    rodin_key = env.get("RODIN_API_KEY", "").strip()
    if not tripo_key:
        raise SystemExit("TRIPO_API_KEY missing in .env")
    if not rodin_key:
        raise SystemExit("RODIN_API_KEY missing in .env")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    log(f"output dir: {OUT_DIR}")
    log(f"assets: {len(ASSETS)} (concurrent)")

    results: dict[str, str] = {}
    with ThreadPoolExecutor(max_workers=len(ASSETS)) as pool:
        futs = {
            pool.submit(run_one, aid, prov, prompt, tripo_key, rodin_key): aid
            for aid, (prov, prompt) in ASSETS.items()
        }
        for fut in as_completed(futs):
            aid, status = fut.result()
            results[aid] = status

    ok = [a for a, s in results.items() if s == "ok"]
    skipped = [a for a, s in results.items() if s == "skipped"]
    failed = [a for a, s in results.items() if s not in ("ok", "skipped")]

    log("--- summary ---")
    log(f"ok ({len(ok)}): {', '.join(sorted(ok)) or '(none)'}")
    log(f"skipped ({len(skipped)}): {', '.join(sorted(skipped)) or '(none)'}")
    log(f"failed ({len(failed)}): {', '.join(sorted(failed)) or '(none)'}")
    for a in sorted(failed):
        log(f"  {a}: {results[a]}")

    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
