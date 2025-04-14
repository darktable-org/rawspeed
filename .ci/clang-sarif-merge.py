#!/usr/bin/env python3

import argparse
import functools
import glob
import json
import os


def create_parser():
    parser = argparse.ArgumentParser(description="merge many clang sarif's")

    parser.add_argument(
        "-o",
        "--output",
        dest="output",
        required=True,
        help="the merged SARIF file",
    )

    parser.add_argument(
        "dir",
        help="Directory containing individual sarif's",
    )

    return parser


def merge_two_sarifs(x, y):
    assert x.keys() == y.keys()
    res = dict()
    for key in x.keys():
        a = x[key]
        b = y[key]
        assert type(a) == type(b)
        if isinstance(a, str):
            assert a == b
            res[key] = a
        elif isinstance(a, list):
            res[key] = a + b
        elif isinstance(a, dict):
            res[key] = merge_two_sarifs(a, b)
        else:
            assert False
    return res


def main():
    # Parse the command line flags
    parser = create_parser()
    args, unknown_args = parser.parse_known_args()
    assert not unknown_args
    files = glob.glob(os.path.join(args.dir, "*.json"), recursive=True)
    datas = []
    for f in files:
        with open(f, "r") as f:
            datas.append(json.load(f))
    result = dict()
    if len(datas) > 0:
        result = functools.reduce(merge_two_sarifs, datas)
    if "runs" in result:
        result["runs"] = [functools.reduce(merge_two_sarifs, result["runs"])]
    with open(args.output, "w") as f:
        json.dump(result, f)


if __name__ == "__main__":
    main()
