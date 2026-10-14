// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

#pragma once
#include "player.h"
namespace mdx {
std::filesystem::path archiveCopyPath(const std::filesystem::path &path);
// True for browsable ZIP/LZH/LHA containers (MRZ remains a track).
bool isArchive(const std::filesystem::path &path);
std::filesystem::path archiveSibling(const std::filesystem::path &source, const std::filesystem::path &name);
// Archive paths remain stable user-facing paths: archive.zip/subdirectory/file.mdx.
std::filesystem::path resolveArchivePath(const std::filesystem::path &path);
uintmax_t archiveFileSize(const std::filesystem::path &path);
std::filesystem::path openMrz(const std::filesystem::path &path,
                              bool materialize = true);
Bytes archivePrefix(const std::filesystem::path &path, size_t limit);
} // namespace mdx
