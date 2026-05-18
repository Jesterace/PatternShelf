# PatternShelf

PatternShelf is a personal cross-stitch pattern library manager built with Qt/C++.

It helps track pattern PDFs, stitch sizes, fabric planning, DMC colors, and missing colors based on a FlossKeeper stash file.

## Features

- Add, edit, delete, and open pattern PDFs
- Import pattern folders recursively
- Import DMC colors from nearby CSV legend files
- Search patterns
- Filter by status:
  - All Patterns
  - Backlog
  - Not Started
  - In Progress
  - Finished
  - Missing Colors
  - Ready to Stitch
- Track stitch width and height
- Calculate design size
- Calculate fabric cut size with configurable border
- Track DMC color lists
- Normalize DMC colors
- Count unique DMC colors
- Compare pattern colors against FlossKeeper stash
- Show have/missing color counts
- Copy a need-to-buy list to the clipboard
- Polished pattern details panel
- Local Linux desktop launcher

## FlossKeeper stash path

PatternShelf currently checks this file:

```text
/home/jared/FlossKeeperSync/flosskeeper_collection.tsv
