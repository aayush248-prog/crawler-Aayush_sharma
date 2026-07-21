# Daily Work Log

**Project:** Web Crawler Design and Development  
**Author:** Aayush Sharma  
**Date:** ___________________

---

# Objective

The objective of today's work was to research, analyze, and design the architecture of a scalable single-machine web crawler. The primary focus was to understand the complete crawling process, identify the major software components, select appropriate data structures, design the Fetcher subsystem, and prepare the overall software architecture for implementation.

---

# Work Performed

## 1. Research on Web Crawler Architecture

The session began by studying how modern search engines discover webpages across the Internet. The complete crawling pipeline was analyzed to understand how each module interacts with others during the crawling process.

The following major components were identified:

- Seed URLs
- Frontier
- Seen Store
- Fetcher
- Link Extractor
- URL Normalizer
- Page Storage
- Crawler Controller

The responsibilities of each module were documented to ensure a modular and maintainable architecture.

---

## 2. Selection of Crawling Strategy

Different graph traversal algorithms were studied for webpage discovery.

Breadth-First Search (BFS) and Depth-First Search (DFS) were compared based on their crawling behavior.

Breadth-First Search (BFS) was selected because it:

- Explores webpages level by level.
- Naturally maintains crawl depth.
- Provides better website coverage.
- Prevents the crawler from exploring one branch too deeply while ignoring other webpages.

---

## 3. Frontier Design

The Frontier module was designed to maintain URLs waiting to be crawled.

Instead of using the Standard Template Library Queue, a **custom Queue implementation using a dynamic array** was selected.

A custom structure was designed to store both the webpage URL and crawl depth together.

```cpp
struct URLDepth
{
    string url;
    int depth;
};
```

The Frontier supports the following operations:

- Push URL
- Pop URL
- Peek Front URL
- Check Empty
- Return Queue Size

This design naturally supports Breadth-First Search traversal.

---

## 4. Seen Store Design

The Seen Store was designed to prevent duplicate crawling.

Instead of using `std::unordered_map`, a **custom HashMap implementation** was selected.

The Seen Store performs the following operations:

- Check whether a URL already exists.
- Insert newly visited URLs.
- Prevent duplicate webpage downloads.

Using a HashMap provides approximately **O(1)** average lookup and insertion time.

---

## 5. URL Normalization

Different URL formats referring to the same webpage were studied.

Normalization rules were defined before inserting URLs into the Seen Store.

The normalization process includes:

- Convert hostnames to lowercase.
- Remove trailing slashes.
- Remove empty query strings.
- Convert relative URLs into absolute URLs.

These rules significantly reduce duplicate webpage crawling.

---

## 6. Fetcher Architecture

A complete Fetcher subsystem was designed to isolate all networking-related operations.

The Fetcher was divided into the following components:

- FetchManager
- URL Validator
- HTTP Client
- Request Builder
- Response Parser
- Retry Handler
- Delay Controller

The Fetcher validates URLs, creates HTTP requests, downloads webpages, processes server responses, retries temporary failures, introduces polite crawl delays, and returns HTML content back to the crawler.

This modular design improves maintainability and allows independent testing of networking functionality.

---

## 7. Link Extraction

Different techniques for extracting hyperlinks from HTML were explored.

Regular Expressions (Regex) were studied to identify hyperlinks using the `href` attribute.

The extracted hyperlinks are later normalized and inserted into the Frontier if they have not already been visited.

The possibility of implementing the Knuth-Morris-Pratt (KMP) string matching algorithm for efficient HTML pattern searching was also explored.

---

## 8. Page Storage Design

The Page Storage module was designed to store downloaded webpages locally.

The storage design includes:

- Saving HTML files in a dedicated directory.
- Mapping webpage URLs to stored filenames.
- Preserving downloaded pages for future indexing.

This module provides the interface required by later indexing components.

---

## 9. Crawler Controller Design

The overall crawler workflow was finalized.

The controller repeatedly performs the following operations:

1. Remove the next URL from the Frontier.
2. Check whether it already exists in the Seen Store.
3. Download the webpage using the Fetcher.
4. Store the downloaded HTML.
5. Extract hyperlinks.
6. Normalize extracted URLs.
7. Insert newly discovered URLs into the Frontier.
8. Continue until the Frontier becomes empty or crawl limits are reached.

This workflow coordinates all crawler components efficiently.

---

## 10. Error Handling

Possible failure conditions were analyzed, including:

- Invalid URLs
- Network failures
- HTTP request failures
- Duplicate webpages
- Empty Frontier
- Maximum crawl depth

Appropriate handling strategies were planned to ensure the crawler continues execution even when individual pages cannot be downloaded.

---

## 11. Complexity Analysis

The expected time complexities of major components were analyzed.

| Component | Average Complexity |
|-----------|-------------------|
| Frontier Push | O(1) |
| Frontier Pop | O(1) |
| Seen Store Lookup | O(1) |
| Seen Store Insert | O(1) |
| Link Extraction | O(n) |

The selected data structures ensure efficient crawling even for a large number of webpages.

---

## 12. Testing Strategy

The following testing approaches were planned:

- Unit Testing
- Integration Testing
- Performance Testing
- Failure Testing

These tests will verify the correctness and robustness of individual modules as well as the complete crawler.

---

## 13. Future Improvements

Potential enhancements identified for future versions include:

- Multithreaded crawling
- robots.txt support
- Priority-based Frontier
- Distributed crawling
- Incremental crawling
- HTML compression
- Improved URL canonicalization

---

# Outcome

Today's work successfully completed the research and software design phase of the web crawler.

The architecture of the crawler was finalized, including the Frontier, Seen Store, Fetcher, Link Extractor, URL Normalizer, Page Storage, and Crawler Controller. Appropriate data structures were selected, the crawling workflow was documented, and the complete design was prepared for implementation in the next development phase.