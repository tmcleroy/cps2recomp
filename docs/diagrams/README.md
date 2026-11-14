# Online diagrams

These diagrams use Morphus56K’s website design: Space Grotesk headings, Inter body text,
JetBrains Mono labels, dark green surfaces, phosphor-green connections and cyan annotations.
They are drawn SVGs with outlined lettering, so GitHub displays the intended fonts without
external font requests. They contain no generated photographic artwork or game assets. Shallow projected faces distinguish
records, snapshots and packet queues; all labels stay front-facing. Seeded Matrix rain is faint and excluded
from reading areas. Geometry shows structure, not measured queue capacity or a physical hardware layout.

| Diagram | Meaning |
| --- | --- |
| [Four machines](mvsc-four-machines.svg) | Protocol-10 topology, per-machine input history, control packet field groups, host ownership validation, rollback limits and MVC fighter-panel routing. The raised history tiles are schematic, not a captured trace. |
| [Recovery and reset](online-recovery-checkpoint.svg) | Historical `4b578bf`: `gone` / `polledGone` snapshots through failed requests and concurrent departures; `Leave(reopening)` intent; `GaugeBoot` and serialized `CFVS` phase. |
| [Upstream integration](online-upstream-integration.svg) | Version reconciliation at `471f7f7`, marked MVC state sections, spectator snapshot/input/ACK isolation, `SetAgrees()` over every playing link acknowledged mode-event retention, monotonic result readiness and generation-safe invitation replies. |

Labels were checked against the depicted revisions: `4b578bf` for the earlier checkpoint, `471f7f7` for the transport and base integration, and `eea67d1` for the upstream version pair. The integration
figure separately identifies result-ordering fix `6dff752` and invitation-race fix `b394926`. Their targeted
protocol and Worker checks are recorded in [November 14 verification](../ONLINE_TESTING.md#targeted-verification-november-14-2026).
Packet field groups are an overview rather than a byte-offset specification.

The diagrams explain behavior, not completed internet playtests. The current gameplay evidence
remains attached to `471f7f7`; the earlier checkpoint retains its own results. A real four-machine
internet match and rematch with physical controllers remains outstanding.

## Design references

- [Huang, *An Eye Tracking Study into the Effects of Graph Layout*](https://arxiv.org/abs/0810.4431):
  the preliminary study found small-angle crossings could slow path searches. These figures keep
  routes uncrossed and use clear branching or merging points.
- [Kim et al., *Towards Visualization Thumbnail Designs that Entice Reading Data-driven Articles*](https://arxiv.org/abs/2305.17051):
  the study examines how visual elements and concise labels contribute to attraction and understanding.
  These figures use short labels next to the relevant controls, events and connections.

These are design applications, not findings tested on this networking implementation.

Text equivalents and detailed limits remain in [Online play](../ONLINE.md#four-players-on-four-machines)
and [the testing record](../ONLINE_TESTING.md).
