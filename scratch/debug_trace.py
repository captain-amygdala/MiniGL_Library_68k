import subprocess

cmd = "/home/claude/.local/bin/vamos -H disable -m 65536 -C 68040 -s 512 scratch/render_mock_triangle"
out = subprocess.run(cmd, shell=True, capture_output=True, text=True)
print(out.stdout)
print(out.stderr)
