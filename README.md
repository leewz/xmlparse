# xmlparse
I made an XML parser in 2013.

This parser was created to parse CBLoader's XML files for the sake of analysis and search.

On my computer, it originally took around 12 seconds to parse a 50+ MB file. I later optimized it down to about 3 seconds, with a custom string class (indexed into an existing string) and a custom allocator .

I then tried Python, where four lines parsed the file in 4 seconds, and the REPL allowed me to write new queries without recompiling and reparsing. I rarely used C++ since.
