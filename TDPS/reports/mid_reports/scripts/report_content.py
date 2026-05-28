"""Shared metadata and content for TDPS initial design report generation."""
from __future__ import annotations

import re
from datetime import date

COURSE_NAME = "Team Design Project and Skills"
REPORT_TITLE = "Initial Design Report - Autonomous Patio Robot"
PROJECT_TITLE = "Initial Design Report for TDPS Autonomous Patio Robot"
TEAM_NAME = "Team [X] - [Name]"
TEAM_NUMBER = "[X]"
TEAM_MEMBERS = [
    ("Name 1", "1234567A"),
    ("Name 2", "1234567B"),
    ("Name 3", "1234567C"),
]

CURRENT_DATE = date.today().strftime("%d %B %Y")
MAX_TECHNICAL_WORDS = 1500

PAGE2_HEADING = "Purpose, Architecture, and Hardware Justification with Schematic Evidence"
PAGE2_PARAGRAPHS = [
    (
        "The project objective is robust autonomous motion on the TDPS Patio line-following map with three mandatory "
        "capabilities: stable PID tracking, radar-based obstacle response, and LoRa telemetry. To justify design "
        "decisions with traceable evidence, this report references three hardware sources: Schematic V1, Schematic V2, "
        "and PCB2 layout. The implementation sequence was requirements and interfaces (Week 1 to 2), electrical "
        "partition and PCB iteration (Week 3 to 5), then closed-loop firmware integration under realistic load conditions."
    ),
    (
        "Figure 1 separates the architecture into a power path and a data/control path. The LM2596S provides the 5 V "
        "driver rail, while AMS1117-3.3 isolates MCU and communication loads. STM32F407 receives line-sensor and "
        "encoder feedback, computes PID output, and schedules LoRa plus Radar tasks. The accompanying V1/V2 power crops "
        "show that this chain is physically implemented, not only conceptual, and that protection and routing were "
        "tightened in the second revision."
    ),
    (
        "Compared with V1, the V2 front end adds LM74700 and NTMFS5C628NLT1G to address reverse polarity and surge risk "
        "before the buck stage. Hardware selection therefore follows two logic paths: mandatory modules (LoRa and Radar) "
        "must be integrated with low interference, while team-selected devices (DRV8874 and the two-stage regulator chain) "
        "must meet thermal margin, current demand, and control quality targets on curved segments."
    ),
]

HARDWARE_TABLE_HEADERS = ["Subsystem", "Selected Device", "Justification"]
HARDWARE_TABLE_ROWS = [
    (
        "Power Protection",
        "LM74700 + NTMFS5C628NLT1G",
        "Blocks reverse insertion and suppresses surge stress at battery input.",
    ),
    (
        "5V Step-down",
        "LM2596S",
        "Efficient 3 A class conversion for motor driver and actuator side loads.",
    ),
    (
        "3.3V LDO",
        "AMS1117-3.3",
        "Separates MCU and LoRa/Radar logic supply from switching ripple sources.",
    ),
    (
        "Motor Driver",
        "2x DRV8874PWPR",
        "Low R_DS(on) with integrated current feedback for closed-loop torque control.",
    ),
]

PAGE2_CALCULATION = (
    "Thermal check: P_D = (V_in - V_out) x I_load = (5.0 - 3.3) x 0.5 = 0.85 W for AMS1117. "
    "With local copper spreading and stitched thermal vias, this operating point is acceptable for current bench trials."
)

PAGE3_HEADING = "Mandatory Sensor Integration, Driver Interface, and Control Strategy"
PAGE3_PARAGRAPHS = [
    (
        "LoRa and Radar are mandatory TDPS modules, so the key design task is robust integration. In V2, LoRa SPI and "
        "Radar UART traces are kept away from DRV8874 PWM switching paths, and return current is managed to reduce RF and "
        "digital coupling. Local decoupling is placed close to communication connectors to limit high-frequency ripple."
    ),
    (
        "The software controller follows a finite-state sequence: initialization, PID tracking, obstacle decision, telemetry, "
        "and resume. Radar detection can preempt propulsion commands and trigger controlled deceleration before returning "
        "authority to the line-following loop. Control updates are scheduled deterministically, while telemetry packaging is "
        "deferred to lower-priority slots."
    ),
    (
        "Schematic V2 confirms IO partitioning: motor and encoder return paths remain grouped, while communication headers "
        "remain in the logic area. Initial Patio trials show loop execution below "
        "1 ms and a useful radar threshold near 0.35 m; the next step is filtered derivative with bounded incremental update "
        "for repeatable corner damping."
    ),
]

PAGE4_HEADING = "Progress Summary, PCB2 Layout Evidence, and Initial Lab Notebook Data"
PAGE4_PARAGRAPHS = [
    (
        "The project is in controlled integration. PCB V2 has been assembled, and baseline procurement coverage is complete for "
        "core subsystems. Initial bench checks confirm stable startup, clean regulator behavior under nominal load, and consistent "
        "motor response for extended Patio-map testing."
    ),
    (
        "The PCB2 layout crop is included as implementation evidence of routing updates in the second iteration, especially around "
        "power entry, driver grouping, and communication connector placement. This physical evidence supports the V1 to V2 design "
        "trade-off discussion rather than relying on abstract claims."
    ),
    (
        "The notebook table below reports representative electrical and control data for the current stage. Values are early engineering "
        "measurements and will be refreshed after repeated trials with synchronized telemetry, radar logs, and corner-case stress tests."
    ),
]

INITIAL_TEST_TABLE_HEADERS = ["Test Item", "Value", "Observation"]
INITIAL_TEST_TABLE_ROWS = [
    (
        "Motor PWM Frequency",
        "20 kHz",
        "Audible motor noise is effectively eliminated and low-speed behavior is smoother.",
    ),
    (
        "LoRa RSSI at 10 m",
        "-65 dBm",
        "Stable telemetry in indoor line-of-sight test with no packet drop in sample run.",
    ),
    (
        "Main Control Loop Time",
        "< 1 ms",
        "Meets real-time PID update requirement for Patio map corner transitions.",
    ),
    (
        "Radar Trigger Threshold",
        "0.35 m",
        "Obstacle-stop behavior is repeatable; threshold adaptation remains under review.",
    ),
]

PAGE5_HEADING = "Project Planning (Week 1 - Week 15)"
PAGE5_PARAGRAPH = (
    "Figure 6 provides the updated Gantt plan from Week 1 to Week 15. Milestones are defined as M1 Planning, "
    "M2 Design Freeze, M3 Functional Robot, M4 End-to-End Validation, and M5 Final Delivery."
)


def technical_word_count() -> int:
    """Return word count over technical pages 2 to 5."""
    text_blocks = [
        *PAGE2_PARAGRAPHS,
        PAGE2_CALCULATION,
        *PAGE3_PARAGRAPHS,
        *PAGE4_PARAGRAPHS,
        PAGE5_PARAGRAPH,
    ]
    joined = "\n".join(text_blocks)
    return len(re.findall(r"[A-Za-z0-9_\-]+", joined))
