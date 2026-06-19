import json
import matplotlib.pyplot as plt
import sys

def parse_raw_data(raw_data):
    xs, ys = [], []

    for k, v in raw_data[0].items():
        if k == "Overall":
            continue

        try:
            xs.append(float(k))
            # Convert from nanoseconds per 4 bytes to Gbps
            ns_per_int = float(v)
            bytes_per_ns = 4 / ns_per_int
            gbps = bytes_per_ns * 8 * 1e9 / 1e9
            ys.append(gbps)
        except (TypeError, ValueError):
            continue

    # sort by key
    pairs = sorted(zip(xs, ys), key=lambda p: p[0])
    if not pairs:
        raise ValueError("No plottable (key, value) pairs found in raw_data.")

    return zip(*pairs)


def main():
    # name = "output.json"
    # if len(sys.argv) > 1:
    #     name = sys.argv[1]

    plt.figure(figsize=(8, 5))

    for fn in sys.argv[1:]:
        print("file:", fn)
        with open(fn, "r", encoding="utf-8") as f:
            data = json.load(f)

        raw_data = data.get("raw_data", [])
        x, y = parse_raw_data(raw_data)

        scat = plt.scatter(list(x), list(y), marker=".", s=12, alpha=0.3)
        plt.scatter([], [], marker='o', s=20, color=scat.get_edgecolor(), label=fn.split('.')[0], alpha=1)

    plt.xscale("log", base=2)  # keys on log2 scale
    plt.xlabel("Vector<int> size")
    plt.ylabel("Alloc tput (Gbps)")  # linear by default
    plt.title("Allocation Overhead")
    plt.grid(True, which="both", linestyle="--", alpha=0.4)
    plt.ylim(0, 300)
    plt.yticks(range(0, 301, 100))
    
    plt.legend()
    
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()