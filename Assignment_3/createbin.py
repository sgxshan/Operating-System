import sys

if len(sys.argv) < 5:
    print "wrong argc!"
    sys.exit()


imgf = sys.argv[1]
outf = sys.argv[2]
size = int(sys.argv[3])
block_nums = map(int, sys.argv[4:])

with open(imgf, "rb") as f:
    img = f.read()

print block_nums
print len(img)

data = ""
for b in block_nums:
    if size >= 1024:
        block = img[(b - 1) * 1024 : b * 1024]
        size -= 1024
        data += block
    else:
        block = img[(b - 1) * 1024 : (b - 1) * 1024 + size]
        data += block
        break

print len(data)

with open(outf, "wb") as f:
    f.write(data)
