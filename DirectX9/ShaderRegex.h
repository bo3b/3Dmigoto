#pragma once

#include "CommandList.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include <pcre2.h>

bool apply_shader_regex_groups(std::string *asm_text, std::string *shader_model, UINT64 hash, std::wstring *tagline);

typedef std::set<std::string> ShaderRegexTemps;
typedef std::set<std::string> ShaderRegexModels;

class ShaderRegexPattern {
public:
	pcre2_code *regex;
	std::string replace;

	bool do_replace;

	// These will be used later when we implement our own advanced
	// substitution to allow matches to be used between multiple patterns
	// in the one regex group, and to apply some (very) simple arithmetic
	// to convert byte offsets to constant buffer indexes and vice versa
	std::set<std::string> named_capture_groups;

	ShaderRegexPattern();
	~ShaderRegexPattern();

	bool compile(std::string *pattern);
	bool named_group_overlaps(ShaderRegexTemps &other_set);
	bool matches(std::string *asm_text);
	bool patch(std::string *asm_text, ShaderRegexTemps *temp_regs, unsigned dcl_temps);
};

// These are sorted to make sure we get consistent results between runs
// in case the user does something that winds up depending on the order:
typedef std::map<std::wstring, ShaderRegexPattern> ShaderRegexPatterns;
typedef std::vector<std::string> ShaderRegexDeclarations;

class ShaderRegexGroup {
public:
	std::wstring ini_section;

	ShaderRegexPatterns patterns;

	ShaderRegexDeclarations declarations;
	ShaderRegexModels shader_models;
	ShaderRegexTemps temp_regs;

	CommandList command_list;
	CommandList post_command_list;

	void apply_regex_patterns(std::string *asm_text, bool *match, bool *patch);
	void link_command_lists(UINT64 shader_hash);
};

// Sorted to make sure that we always apply the regex patterns in a consistent
// order, in case the user writes multiple patterns that depend on each other:
typedef std::map<std::wstring, ShaderRegexGroup> ShaderRegexGroups;
extern ShaderRegexGroups shader_regex_groups;
