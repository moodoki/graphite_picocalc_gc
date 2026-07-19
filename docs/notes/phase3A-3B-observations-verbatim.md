Usage notes:

1. '?' as shortcut to help instead of typing full help as a commant alias for convenience
2. shorten 'stats' and 'lists' to 'stat' and 'list' i type that often, no command collision as here
3. lists broadcast works with list variables but does not work with lists typed in braces, error
   shows as "Expected a list" with {1, 2, 3} + 2 and "Bad list element" with {1, 2, 3} + {2, 2, 2}
4. Pressing shift+tab (Home) in graph screen seems to break something, repeatedly pressing esc no
   longer gets back to home screen after this and home screen become inaccessible. Home should
   bring us back to the home screen but it brings us to the graph screen instead
5. a range() function to quickly generate a list with known start, end, and step size will be
   useful. this should work with other list math that we have. 
6. regressions work, saving to y= works
7. large arrays is hard to test now without quick ways of generating them. 
8. mean, median, std should have home screen commands
9. Computing indicator should be added, since we can't really test for feel now, no harm adding

UI notes:
1. replace 'pi' in pretty print math with greek letter pi if glyph exists in our current font
2. negative numbers with dark grey in list editor is too dark, keep them the same color as positive
   numbers
3. consider using greek letters for mean, std, sum. does our display support subscripts yet? we may
   want to consider this for nicer stats display..
4. consider replacing fonts with JuliaMono, we may need to bake this ourselfs from the files
   online, but this has more complete math glyphs, needs to check licensing


Some good to haves:
1. scientific constants (seems easy to fold into any session, we just need to decide how to expose
   these, decide at the start of session)
2. unit conversions, maybe as part of apps later, or use designs similar to scientific calculators.
   design effort necessary, differ decisions to later
3. kiv expanding beyond 6 lists, and loading of list data from data files on sdcard, may be useful
   for future CBL/CBR expansion

