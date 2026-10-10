"""One fresh, rendered actual-map club audit; does not modify saved map/settings.

Audio stays enabled so the native check inspects a playing UAudioComponent,
not only a mathematical attenuation function. The user directory isolates
settings and the campaign save from an editor or the player's normal game.
"""
import argparse
import json
import pathlib
import shutil
import subprocess
import time
import uuid


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--user-dir")
    parser.add_argument("--timeout", type=int, default=530)
    args = parser.parse_args()
    base = pathlib.Path(__file__).resolve().parent
    repo = base.parent.parent
    evidence = base / "evidence"
    evidence.mkdir(exist_ok=True)
    user = pathlib.Path(args.user_dir or f"/tmp/nightclub-audit-{uuid.uuid4().hex[:10]}")
    log = evidence / "runtime-smoke.log"
    command = [
        "/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor",
        str(repo / "SeniorSendoff.uproject"), "/Game/Story/Maps/Chapter01_House",
        "-game", "-windowed", "-ResX=1280", "-ResY=720", "-NightClubSmoke",
        "-UserDir=" + str(user), "-AbsLog=" + str(log), "-NoSplash",
    ]
    # Root sequences this alongside visual and traversal/LAN tests. Refuse to
    # compete for GPU/RAM with another full editor or game benchmark.
    existing = subprocess.run(["pgrep", "-x", "UnrealEditor"], capture_output=True, text=True)
    if existing.returncode == 0:
        raise SystemExit("Another Unreal Editor/game is open; sequence this audit after it exits.")
    print("Starting rendered club audio, floor, and frame-rate audit", flush=True)
    started = time.time()
    with (evidence / "runtime-smoke-stdout.log").open("w") as stream:
        process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT)
        try:
            code = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                process.wait(timeout=12)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            code = "TIMEOUT"
    text = log.read_text(errors="replace") if log.exists() else ""
    source = user / "Saved" / "NightClub" / "verification.json"
    report = json.loads(source.read_text()) if source.exists() else None
    if source.exists():
        shutil.copy2(source, evidence / "runtime-verification.json")
    passed = code == 0 and "NIGHT_CLUB_SMOKE_PASSED" in text and report and report.get("status") == "PASS"
    result = {
        "status": "PASS" if passed else "FAIL", "exit_code": code,
        "seconds": round(time.time() - started, 2), "user_dir": str(user),
        "log": str(log), "report": str(evidence / "runtime-verification.json"),
        "scope": "Actual rendered Chapter01_House collision/audio/lights/FPS; native pole E traversal and LAN test are separate.",
    }
    (evidence / "runtime-run.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2), flush=True)
    if report and report.get("framePacing"):
        for entry in report["framePacing"]:
            print(f"{entry['name']}: {entry['averageFPS']:.1f} FPS; median {entry['medianFrameMs']:.1f} ms; p95 {entry['p95FrameMs']:.1f} ms", flush=True)
    raise SystemExit(0 if passed else 1)


if __name__ == "__main__":
    main()
