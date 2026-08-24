import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("timing_data.txt", sep=r"\s+")

combinations = df[["P", "N"]].drop_duplicates().values

box_data = []
labels = []

for P, N in combinations:
    times = df[(df["P"] == P) & (df["N"] == N)]["time"].values
    box_data.append(times)
    labels.append(f"P={P}\nN={N}")

fig, ax = plt.subplots(figsize=(12, 6))

ax.boxplot(
    box_data,
    labels=labels,
    showmeans=True
)

ax.set_title("Execution Time vs Processes (3D Stencil)")
ax.set_xlabel("Processes (P) and Grid Size (N³)")
ax.set_ylabel("Time (seconds)")
ax.grid(axis='y', linestyle='--', alpha=0.5)

plt.tight_layout()

plt.savefig("boxplot.png", dpi=300)
plt.show()