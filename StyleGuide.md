Suggested coding style.  To be implemented as a clang-format and clang-tidy file in the project, so that we can keep it consistent going forward.  clang-format will be done automatically upon pasting, closing brackets, and saving with no intervention or thought needed. clang-tidy will show Intellisense recommendations that we probably will set any violations to be errors, not just warnings, but are advisory and won't cause compile errors.

I've reviewed 6 or 7 different coding styles, including all the ones specified by clang-format.  There is no one-best way, so this is just my mostly arbitrary choices, but also factoring in how much work it is to implement.  I'm looking for biggest bang for the buck, closest to what we already have for fewer changes/work. When in doubt, I deferred to the[ C++ Core Guidelines from Stroustrup](https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md).

[6 cpp style comparisons](https://github.com/motine/cppstylelineup)
[crosire reshade example](https://github.com/crosire/reshade/blob/main/source/effect_codegen_hlsl.cpp)
[gitbooks best practices](https://lefticus.gitbooks.io/cpp-best-practices/content/03-Style.html)
[style guidelines recommendations](https://style-guides.readthedocs.io/en/latest/cpp.html)

Starting point will be Google coding style, but will have tweaks for our purposes.  Also using Reshade code as a reference as a visually desirable style.  Goal is not be too heroic here, renaming everything in every file is too much. So we want to blend in desirable goals without spending inordinate effort on anything that has lesser value.  If anything gets into taking too much time/effort, it'll be skipped.  The consistency is the big win, not individual formatting pieces.

Style guide with some _why_:

#### Classes
- Class references will remain CamelCase for definitions.  Microsoft objects that we subclass are always CamelCase, so keeping these consistent will make it clear when we are talking about Objects.
- All class methods will be CamelCase.  A lot of these are defined by Microsoft objects that we want to respect. The difference between our methods and Microsoft methods will be clear because of how they are called.
- Field references in objects will be lowerCamelCase style. To remove the m that neither Helifax nor I like, but still keep them in the CamelCase variant to indicate 'object', instead of a local variable. No differences for public vs. private, but lowerSnakeCase will clearly indicate an object field.
- Operator overloads.  Only to be used when it cannot be confused.  These are as risky as bad macros.
- A general goal is to make object references and usages clearly different than regular variables or subroutines.

#### Common
- Local variables and subroutines will be lower_snake_case to identify them as 'normal' code.
- Function parameters will be lower_snake_case. We will make exceptions for pCamelCase, and ppCamelCase though, because they are just too common here.
- structs will be lower_snake_case, and we should never use struct as class definitions.
- Macros will be MACRO_TYPE style, including all the logging. 
- Constants. Always define using constexpr.  Will be lower_snake_case like `pi=3.14` or `end_of_line=42;`. We won't visually differentiate between constants, enums, or variables, because the compiler will catch misused variants.
- Template parameter names should match their use case. Use T as the convention for the parameter name.
- Anything not specified should be in lower_snake_case.

#### Headers
- Rename .h files to .hpp 
- Never use _using namespace_ in header files.  We don't use namespaces much, but I'll check this.
- Prefer to always avoid _using namespace_ even in cpp files, in favor of specific uses like _using std::vector;_
- Prefer to always use specific _using std::string;_ clause instead of inline _std::..._
- Always include `#pragma once` guards in headers.  We already do this I think.
- Use "" for local headers versus <> for system/SDK headers.  I'm sure there are some errors here.

#### General
- We want to avoid hungarian notation as most of us don't care for it.  Some exceptions will be skipped like pCamelCase input parameters.
- Initialize all variables preferably with `{}` style.  Probably not doable, but Resharper might catch all missing ones.
- Avoid templates, except where inputs are actually unrelated objects. Avoid using template polymorphism. Templates bugs are really difficult, because the compiler runs amok. Also I personally despise the syntax.  Still, good to use in narrow circumstances. 
- Never use `auto`.  This is just putting the onus on the reader, and it's much better clarity to just use the type.
- Always use nullptr instead of NULL or 0 for pointers.  It clarifies that the reference is a ptr.
- Use Rule of Zero.  Don't add stuff we aren't actively using just to be 'complete.'  Like every possible constructor or every possible overload or override.
- Use TODO in comments as reminders for unfinished code.
- Always check error results, just as a good habit.  If it's something 'impossible', have it throw a fatal error using a fatal macro.

#### Formatting
- Using spaces for whitespace, instead of tabs.  Spaces are generally preferred (Stackoverflow), but the whole project is already tabs set to 4. Mild preference for spaces, so we'll switch to that. Especially because we plan to use no wrapping, this is much less important.
- No line wrapping except for comments.  Code is typically not read like English, and most of a very long input parameter list, or other long lines are to be skimmed unless looking at bugs.  Wrapping makes code much harder to read, because the wrap is at the same importance as the start, which is not accurate.
- Formatting will be Allman brace style.
- Single line if/else without braces are allowed
- Pointer definitions will be left justified. e.g. `HackerDevice* hacker` Because it is part of the type definition itself.  This is the opposite of current code base. I personally find the right justified approach super confusing.
- Comment blocks prefer // always, and each line limited to 80 chars.  Since this is English, having a line limit helps readability, as narrow columns are easier to read.  
- We need documentation. Comments should only ever be explaining _why_.  The _what_ is explained by the code. Anything that takes longer than 3 hours to figure out deserves a comment to help future-you and others on the team. Write comments as you code- you will never come back and add them. For what it's worth, my approach is to write comments first, as a way of telling myself in English what I'm going to do in code.

<br>
-----
<br>

Some notes based on research. Hard to understand details of tools.

clang-format has limited abilities, not enough detail in specifications to get exactly what I'd want.  In particular, the function parameters for definitions cannot be wrapped, if the line limit is 0.  There are no flags to wrap those.  In addition, setting it low to like 40, which forces wraps then breaks the ability to align assignments. Not enough flexibility.  So, we'll leave it at 0, and format those using Resharper for a first pass, and manually later on.

Do not use VS2017. The clang support is much more limited there, and the clang-tidy options are also missing key aspects we want. VS2019 was used by default here, but moving to VS2022 is likely.

<br>
-----
<br>

#### Discussions

> That all looks good to me, cheers Bob. Regarding templates, are we against using templated functions for subclasses? There are quite a few instances in the code where I have used a template instead of a virtual function in the (perhaps misguided) belief that it will compile to fewer instructions. I know there are maintenance of costs of doing this and am by no means wedded to this tactic, but just thought now would be the time to mention it.

OK, sounds good.  I'm going to make an example of an 'after' and post it here so we can see what it looks like before doing the full pass.  My initial reaction is that I really like how it looks refactored and renamed this way, and it makes the code much more readable.  Kind of gives me extra enthusiam to get this done because we've just put up with the crazy mixed styles for years.    
  
---

Regarding the template polymorphism- I'll start by saying that I have very limited experience with templates, so I'm not a particularly great person to decide.  Also I feel that templates tend to get abused in C++, which puts me in the biased category as well.  I'm not completely opposed, I just think it's often the wrong tool for the job, at least partly because debugging them sucks, and the syntax sucks.  Templates serve a valid and consistent purpose for stuff like handling maps and lists, where you want to store unknown items. 

So having noted that... I'd really prefer if we don't use templates for polymorphism, and stick with conventional object/class dynamics.  There isn't going to be any performance advantage. That would be premature optimization, unless it's been profiled and found that dispatching through a vtable is measureable.  

Here's a good short writeup that I used to refresh my memory: https://www.stubborncoder.com/2021/05/03/templates-vs-virtual-functions/

I hate to say no, and am open to reasons why we ought to use them, but this falls into my category of something I personally find really annoying in code, and it would grind my gears always in the future.  I could understand a project where a team decided to use template polymorphism only, but having them mixed in the same project I think would be a mistake.  Using templates to replace virtual functions just seems like one of those C++ things that is full of sharp edges and endless problems.  But... I am super vanilla man.  I prefer code to be as dumb and simple and lame as possible.

It's a great question, and since you are not wedded to the idea, I think we should forgo that approach.  I'll add it as part of our style guide.

> Have to note that there are a few places in 3DMigoto where we use templated functions almost by necessity to ... compensate ... for Microsoft's DirectX API design, in particular all the functions that are duplicated for each type of shader (CreateVertexShader, CreatePixelShader, CreateXXXShader, VSSetShader, etc, etc, etc), since the only difference in these functions is the shader types (and IIRC there might have been other examples to handle different resource desc types, etc), which don't derive from a common base class (thanks Microsoft...) and so as far as the compiler are concerned are incompatible with one another and cannot be cast. I'm not a particular fan of templates, but they allow us to write our code once and fix our bugs once instead of the six times we had before.
> 
> That said, if there's a better solution to this problem that doesn't involve templates or any other nasty C++ constructs I wouldn't be opposed to switching - though I'd caution against making sweeping changes to existing code just for the sake of eliminating templates due to the risk of regressions that brings.

We definitely won't be do any sort of wholesale changes to the current setup, just to avoid templates.  It's working fine, and unless we have a compelling reason to switch I don't think there is any harm in using them for the shader management.

I do think we should avoid them as much as possible going forward though, because having both kinds of polymorphism just adds undesirable complexity. And I think we are better served by keeping to the Microsoft structure, even as we agree it's not particularly great.  We are stuck with DX11, and trying to work around their designs I think is not the best approach because it adds complexity.

----

Having noted that however- it was always my goal to have specific shader overrides done on a C++ object level, so that any given shader would know everything it needed to know to do its job, including the original shader, and the replacement.  With this model, there is no need to do shader searching through a mapped set of shaders or manage them via hash code, each object would ideally know and have a reference to everything needed.  

> As I needed to wrap all the shaders anyway, shader override pointers are now passed into the shader constructor, doing away with the map search on draw. This was causing a crash on F10 reload, as existing shaders were then pointing at deallocated overrides, but have fixed this in the latest update.

The current approach using templates is good for coding, but bad for performance, because now everything needs to go through the lookup_shader_map.  The last times I profiled 3Dmigoto quite awhile ago, the map function was taking a measurable and concerning amount of performance.  Not critical, but we are definitely above my 5% overhead criteria, last I checked.

Academic for now.  Unless we hit a particular game or scenario where we prove out a performance problem, there is no need to switch now, even as we are starting semi-fresh.

3-26-25: Random thought. We can't use C++ inheritance, because the C++ objects are not COM objects. Based on discussion, what if instead of using C++ objects, we made everything COM objects instead? That would allow us to use standard OO approaches, with a consistent approach, instead of this clunky hybrid.

----

##### 3-26-25 Update:

Coming back to this after a long hiatus. Goal is to finish this up and integrate into master.  Will most likely archive 1.3.16, then rename Rename_Reformat branch to master. Version change to 2.0 as having changed a lot, including toolset, new additions, and massive refactoring. Very likely to move to VS2022.

One cleanup edit here- it's not proper C++ to use initializers like:  
  ```shader_ins ins = {0};```  
This works, but is doing two different types of initialization at once. The first element of the struct is cleared specifically to zero, then the rest of the elements get a default zero initialization. In practice identical for plain old data, but it's not considered good form. The only reason to use 0 there is to be explicit, and so let's go to the industry standard of:  
  ```shader_ins ins = {};```  

Will change to never use _using namespace_ even in cpp files, in favor of specific uses like _using std::vector;_  This is the current best practice recommendation for clarity. Anything in _std::_ should be named in a _using std::..._ clause to avoid inline noise of obvious, standard, data types. For items that are pretty rare, like _std::wistringstream_, we'd prefer inline usage as opposed to the _using_ clause. Like a lot of this, this is a bit arbitrary. If something only has a single use, inline is better. In .h or .hpp files we want to never use 'using' of any form because it is then used in cpp files.

Would like to move everything to using ComPtr<>, which would simplify our memory management and clean up some code. It's also the recommended approach for anything DX11. It adds some line noise, but the automatic dispose upon losing scope is worth the price of admission.
