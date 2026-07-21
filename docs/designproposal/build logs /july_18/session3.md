# Session 3 (2:00 PM – 4:00 PM)

**File:** `Session_2_4.md`

## Objectives
- Design the Fetcher subsystem.
- Understand webpage downloading.

## Work Completed
- Designed FetchManager, URL Validator, HTTP Client,
  Request Builder, Response Parser,
  Retry Handler and Delay Controller.
- Planned retry strategy and polite crawling delay.
- Designed Fetcher request-response workflow.

## Design Decisions
- Isolate networking inside Fetcher.
- Retry temporary failures.
- Continue crawling after permanent failures.

## Outcome
Completed the Fetcher architecture.
