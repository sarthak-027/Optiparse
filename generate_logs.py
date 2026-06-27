import random

# Generates a 500MB file with random logs
target_word = "ERROR"
lines_to_write = 10_000_000

with open("server_logs.txt", "w") as f:
    for i in range(lines_to_write):
        if random.random() < 0.05: # 5% chance to be an error
            f.write(f"2026-03-31 14:00:00 [SYSTEM] {target_word}: Memory spike detected at node {random.randint(1, 100)}\n")
        else:
            f.write(f"2026-03-31 14:00:00 [SYSTEM] INFO: Health check passed for node {random.randint(1, 100)}\n")

print("Massive log file created successfully!")