from _pi.lib import pi_approx

approx = pi_approx(10)
print(approx)
assert str(approx).startswith("3.")

approx = pi_approx(10000)
print(approx)
assert str(approx).startswith("3.1")

