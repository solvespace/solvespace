//-----------------------------------------------------------------------------
// Discovery and loading of our resources (icons, fonts, templates, etc).
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------

#ifndef __RESOURCE_H
#define __RESOURCE_H

// Only the following function is platform-specific.
// It returns a pointer to resource contents that is aligned to at least
// sizeof(void*) and has a global lifetime, or NULL if a resource with
// the specified name does not exist.
const void *LoadResource(const std::string &name, size_t *size);

std::string LoadString(const std::string &name);

#endif
