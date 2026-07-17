-- fix-links.lua
--
-- Pandoc Lua filter for arm_can_tool's mkepub.sh.
--
-- PURPOSE
--   1. Rewrites cross-file markdown links into "#<id>" anchors so
--      they work as internal navigation inside the combined EPUB.
--   2. Rewrites relative asset links (images, PDFs, etc.) to absolute
--      GitHub URLs so they remain reachable from inside the EPUB.
--   3. Reports every link that cannot be resolved to stderr, grouped
--      by severity, so broken references can be found and fixed
--      iteratively before the epub is published.
--
-- USAGE
--   Add to the pandoc command line:
--
--     --lua-filter=fix-links.lua \
--     -M base_url="https://github.com/koendv/arm_can_tool/blob/main/"
--
--   base_url must end with "/".  It is the GitHub blob URL for the
--   repository root.  The filter derives the raw URL for images
--   automatically by rewriting the domain and removing "/blob":
--
--     blob:  https://github.com/<owner>/<repo>/blob/main/<path>
--     raw:   https://raw.githubusercontent.com/<owner>/<repo>/main/<path>
--
--   Image extensions that use the raw URL:
--     .png  .jpg  .jpeg  .gif  .webp  .svg  .svgz
--   Everything else (PDF, epub, source files, ...) uses the blob URL.
--
-- ASSET RESOLUTION
--   For each relative non-.md link the filter searches for the file
--   on disk using the same resource-path directories that pandoc was
--   given via --resource-path (available as PANDOC_STATE.resource_path).
--   The first directory in which the file is found determines the
--   repo-relative path that is appended to base_url / raw_url.
--   If the file is not found in any resource-path directory it is
--   reported as a missing asset in NOTICES and left unchanged.
--
-- ITERATIVE WORKFLOW
--   Run mkepub.sh and inspect stderr.  Three categories are reported:
--
--     ERRORS   -- .md cross-references that could not be resolved.
--                 Fix the link in the source markdown and re-run.
--
--     WARNINGS -- anchors that exist but belong to a different chapter
--                 than the file named in the link (duplicate heading
--                 text, or stale reference after a rename).
--                 Either disambiguate the heading or correct the link.
--
--     NOTICES  -- asset links where the file was not found on disk.
--                 Add the missing file and re-run.
--
--   Repeat until stderr shows only:
--     [fix-links] all cross-references resolved cleanly
--
-- MAINTENANCE
--   Keep CHAPTER_FILES below in the same order as the files are
--   listed in mkepub.sh.  Basenames only -- "doc/" or other path
--   prefixes that appear in links are stripped automatically.
--   That is the only thing that needs updating when chapters are
--   added, removed, or reordered.

local CHAPTER_FILES = {
  "README.md",
  "INSTALL.md",
  "TUTORIAL.md",
  "OPERATION.md",
  "DEBUG.md",
  "TARGETS.md",
  "CANBUS.md",
  "SCRIPT.md",
  "LOGGING.md",
  "HARDWARE.md",
  "SCHEMATIC.md",
  "AI.md",
  "REMOTE.md",
  "DEVELOPER.md",
  "MANUFACTURING.md",
  "LICENSE.md",
  "COMMERCIAL.md",
  "REFERENCE.md",
  "LUA_REF.md",
}

-- ── helpers ──────────────────────────────────────────────────────────

local function basename(path)
  return path:match("([^/]+)$") or path
end

local function is_absolute(target)
  return target:match("^%a[%w+.-]*:") ~= nil
end

local function is_markdown(fname)
  return fname:match("%.md$") ~= nil
end

local IMAGE_EXTS = {
  png=true, jpg=true, jpeg=true, gif=true,
  webp=true, svg=true, svgz=true,
}

local function is_image(path)
  local ext = path:match("%.([^.]+)$")
  return ext and IMAGE_EXTS[ext:lower()] or false
end

-- Search for `path` in each resource-path directory in order.
-- Returns the repo-relative path (dir/path) of the first match,
-- or nil if not found anywhere.
local function find_asset(path)
  for _, dir in ipairs(PANDOC_STATE.resource_path) do
    local full = (dir == "." or dir == "") and path or (dir .. "/" .. path)
    local f = io.open(full, "r")
    if f then
      f:close()
      return full
    end
  end
  return nil
end

-- Derive the raw.githubusercontent.com base URL from a github.com
-- blob URL.  Assumes base_url is of the form:
--   https://github.com/<owner>/<repo>/blob/<branch>/
local function make_raw_url(blob_url)
  local raw = blob_url
    :gsub("^https://github%.com/", "https://raw.githubusercontent.com/")
    :gsub("/blob/", "/")
  return raw
end

-- ── main filter ──────────────────────────────────────────────────────

function Pandoc(doc)

  -- ── Read metadata ──────────────────────────────────────────────────

  local base_url = nil
  local raw_url  = nil

  if doc.meta.base_url then
    base_url = pandoc.utils.stringify(doc.meta.base_url)
    if not base_url:match("/$") then base_url = base_url .. "/" end
    raw_url = make_raw_url(base_url)
  else
    io.stderr:write(
      "[fix-links] CONFIG: -M base_url=<url> not set -- " ..
      "asset links will not be rewritten\n")
  end

  -- ── Pass 1: build lookup tables from the merged document ───────────
  --
  --   chapter_anchor[filename]  "#<id>"    top-level anchor for each chapter
  --   id_chapter[id]             filename  which chapter owns each heading id

  local chapter_anchor = {}
  local id_chapter     = {}
  local chapter_index  = 0

  for _, blk in ipairs(doc.blocks) do
    if blk.t == "Header" then
      local id = blk.identifier
      if blk.level == 1 then
        chapter_index = chapter_index + 1
        local fname = CHAPTER_FILES[chapter_index]
        if fname then
          chapter_anchor[fname] = "#" .. id
          id_chapter[id]        = fname
        else
          io.stderr:write(
            "[fix-links] CONFIG: more level-1 headings than entries in\n" ..
            "  CHAPTER_FILES -- add the extra file to the list\n")
        end
      else
        -- Record every sub-heading.  If pandoc had to disambiguate a
        -- duplicate heading it will have appended "-1", "-2", etc.; all
        -- variants end up here, each mapped to their actual chapter.
        local current = CHAPTER_FILES[chapter_index]
        if current then
          id_chapter[id] = current
        end
      end
    end
  end

  if chapter_index ~= #CHAPTER_FILES then
    io.stderr:write(string.format(
      "[fix-links] CONFIG: found %d level-1 headings but CHAPTER_FILES\n" ..
      "  has %d entries -- these must match\n",
      chapter_index, #CHAPTER_FILES))
  end

  -- ── Pass 2: rewrite links and collect diagnostics ──────────────────
  --
  -- current_chapter tracks which chapter the walker is currently
  -- inside, so diagnostics can say where each bad link came from.

  local current_chapter = "(unknown)"

  -- Accumulated messages, printed together at the end so they do not
  -- interleave with pandoc's own output.
  local errors   = {}   -- .md cross-references that cannot be resolved
  local warnings = {}   -- anchors found but in the wrong chapter
  local notices  = {}   -- asset files not found on disk

  local function err(msg)  errors[#errors+1]   = msg end
  local function warn(msg) warnings[#warnings+1] = msg end
  local function note(msg) notices[#notices+1]  = msg end

  local new_doc = doc:walk {

    -- Track position so diagnostics can name the source chapter.
    Header = function(el)
      if el.level == 1 then
        current_chapter = id_chapter[el.identifier] or el.identifier
      end
      return el
    end,

    Link = function(el)
      local target = el.target

      -- Absolute URLs: leave untouched.
      if is_absolute(target) then return el end

      -- Existing same-document anchor: already correct.
      if target:match("^#") then return el end

      local path, fragment = target:match("^(.-)(#.*)$")
      path     = path     or target
      fragment = fragment or ""
      local fname = basename(path)

      -- ── non-.md relative link: asset ───────────────────────────────
      if not is_markdown(fname) then
        if base_url then
          local found = find_asset(path)
          if found then
            local gh_url = is_image(path) and (raw_url .. found)
                                           or (base_url .. found)
            el.target = gh_url
          else
            note(string.format("  %-20s  %s  (not found in resource-path)",
              current_chapter, target))
          end
        end
        return el
      end

      -- ── .md link: file not in CHAPTER_FILES ────────────────────────
      if not chapter_anchor[fname] then
        err(string.format("  %-20s  %s  (file not in CHAPTER_FILES)",
          current_chapter, target))
        return el   -- cannot rewrite; leave as-is so epub still builds
      end

      -- ── .md link with a fragment ───────────────────────────────────
      if fragment ~= "" then
        local id = fragment:sub(2)
        local owner = id_chapter[id]

        if not owner then
          -- Fragment does not match any heading in the whole document.
          err(string.format("  %-20s  %s  (anchor not found)",
            current_chapter, target))
          el.target = chapter_anchor[fname]   -- fall back to chapter top

        elseif owner ~= fname then
          -- Anchor exists but belongs to a different chapter than the
          -- one named in the link.  Likely a duplicate heading or a
          -- stale reference after a rename.
          warn(string.format(
            "  %-20s  %s  (anchor #%s belongs to %s)",
            current_chapter, target, id, owner))
          -- Rewrite anyway: jumping to the right section is better
          -- than jumping to the wrong chapter's top.
          el.target = fragment

        else
          -- Clean: anchor found in the expected chapter.
          el.target = fragment
        end

        return el
      end

      -- ── .md link with no fragment: link to chapter top ─────────────
      el.target = chapter_anchor[fname]
      return el
    end,
  }

  -- ── Print diagnostics ──────────────────────────────────────────────

  local function banner(label, t)
    if #t == 0 then return end
    io.stderr:write("\n[fix-links] " .. label .. " (" .. #t .. ")\n")
    for _, msg in ipairs(t) do
      io.stderr:write(msg .. "\n")
    end
  end

  banner("ERRORS   -- .md cross-references that could not be resolved", errors)
  banner("WARNINGS -- anchors found but in wrong chapter",              warnings)
  banner("NOTICES  -- asset files not found on disk",                   notices)

  if #errors == 0 and #warnings == 0 and #notices == 0 then
    io.stderr:write("\n[fix-links] all cross-references resolved cleanly\n")
  end

  return new_doc
end
