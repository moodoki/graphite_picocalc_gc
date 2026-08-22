# An SD app that asks, computes and plots (6B.15/6B.16).
#
# The other half of the hello app: this one does NOT call clear_screen,
# so it never takes the screen, and what you see when it ends is the
# output pane with everything it printed. ESC there goes back to the
# launcher, the same as ESC on a canvas.
#
# draw_rect is how you get a clean field without taking the screen —
# only clear_screen does that (D80).

import calc

calc.draw_rect(0, 0, 320, 320, "black", True)
calc.draw_text(0, 0, "Solve a*x^2 + b*x + c = 0", "cyan", "black")

a = float(calc.input("a? ", 30))
b = float(calc.input("b? ", 50))
c = float(calc.input("c? ", 70))

if a == 0:
    print("a must not be zero - that is not a quadratic")
else:
    # The variables go to the calculator, so the expressions below are
    # the ones you would type on the home screen, and they are still
    # set afterwards if you want to keep working with them.
    calc.store("a", a)
    calc.store("b", b)
    calc.store("c", c)

    disc = calc.eval("b^2-4*a*c")
    print("discriminant:", disc)
    if disc < 0:
        print("no real roots")
    else:
        print("x1 =", calc.eval("(-b+sqrt(b^2-4*a*c))/(2*a)"))
        print("x2 =", calc.eval("(-b-sqrt(b^2-4*a*c))/(2*a)"))

    # Plot into a Y= slot and ask for the graph screen. It appears when
    # the script ends, not now — ESC there comes back to this output.
    calc.plot("a*x^2+b*x+c")
    calc.show_graph()
