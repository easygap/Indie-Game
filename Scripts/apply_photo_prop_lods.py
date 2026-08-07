"""Apply the imported photo-prop LOD contract without reimporting source art."""

from __future__ import annotations

import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from photo_prop_lod_contract import apply_photo_prop_lod_contract

import unreal


def main() -> None:
    result = apply_photo_prop_lod_contract()
    unreal.log_warning(
        "PHOTO_PROP_LOD_BUILD PASS "
        f"meshes={result['meshes']} updated={result['updated']} "
        f"large={result['large_props']} small={result['small_props']} "
        f"nanite_review={result['nanite_review_candidates']}"
    )


if __name__ == "__main__":
    main()
