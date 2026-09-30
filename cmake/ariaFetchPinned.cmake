# Use the dependency verifier belonging to our pinned Aria revision.
# Keeping a private copy here would shadow Aria's module and bypass its fixes.
include("${ARIA_DIR}/cmake/ariaFetchPinned.cmake")
