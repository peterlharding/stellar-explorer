# Issues for other projects

Drafts of issues to report upstream, found while building Stellar Explorer.
Nothing here has been posted.
Each draft has a title and a body ready to paste, with a script that reproduces its figures.

| Draft | For | Status |
| --- | --- | --- |
| [hyg-ra-without-cos-dec.md](hyg-ra-without-cos-dec.md) | [astronexus/hyg](https://codeberg.org/astronexus/hyg/issues) on Codeberg | Draft, not posted |
| [hyg-barnard-pmdec-overflow.md](hyg-barnard-pmdec-overflow.md) | [astronexus/hyg](https://codeberg.org/astronexus/hyg/issues) on Codeberg | Draft, not posted |

How the pipeline works around both problems is in [docs/DATA-MODEL.md](../docs/DATA-MODEL.md#hyg) and [ADR 0011](../docs/decisions/0011-astrometry-from-hipparcos.md).

## Before posting

The drafts were written with an AI coding assistant, and Codeberg restricts LLM-generated content (checked 28 October 2026):

- Codeberg's members voted on 23 July 2026, 358 to 144, to add § 2 (1) 7 to its [Terms of Use](https://codeberg.org/Codeberg/org/src/branch/main/TermsOfUse.md): "You must not share projects that mostly consist of code written by "generative AI"-tools (including services such as *Claude*, *OpenAI Codex*)."
  The rule is about hosted projects, not issues.
  Stellar Explorer is on GitHub, so it does not apply to this repository.
- The [blog post announcing it](https://blog.codeberg.org/protecting-our-floss-commons-from-llms.html) also names "users sending LLM contributions that violate a project's custom policies" as unwelcome.
  HYG has no contribution policy: no CONTRIBUTING file and no issue template.
- A [proposal](https://codeberg.org/Codeberg/Community/issues/2914) to ban LLM-generated content in issues, pull requests and comments outright was closed on 23 July 2026 without comment, so it is not in the Terms of Use.

So posting these is within Codeberg's rules today, but only as reports a person stands behind.
Before posting each one:

1. Run its script yourself and check the numbers in the body match.
   Both scripts need only Python 3; they download HYG from Codeberg and Hipparcos from VizieR.
   They also accept a saved copy of the VizieR result as an argument, which is how they were run on 28 October 2026 while CDS was not answering.
2. Read the body and change anything you would not say in your own words.
   The maintainer will reasonably judge an issue by whether its author understands it.
3. Decide whether to say an AI assistant helped.
   It is not required, but given Codeberg's stance it is courteous; one line is enough, for example: "I found this while building a catalogue pipeline with an AI coding assistant; I have checked the figures myself with the script below."
4. Post the right-ascension issue first: it affects every fast star, while the Barnard's Star one is a single row.

After posting, add the issue links to the table above.
