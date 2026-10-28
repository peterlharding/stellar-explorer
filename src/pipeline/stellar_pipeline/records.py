"""What the parsers produce and the writer stores: objects with their claims."""

from dataclasses import dataclass, field


@dataclass(slots=True)
class Measurement:
    quantity: str
    value: float
    source: str
    err_lower: float | None = None
    err_upper: float | None = None
    # 1 when the value is an upper limit, -1 when it is a lower limit.
    limit: int = 0
    epoch: float | None = None
    derivation: str | None = None
    # A value the source gives but that is known to be wrong. Kept for the
    # record, never preferred.
    invalid: bool = False
    note: str | None = None
    reference_label: str | None = None
    reference_url: str | None = None
    preferred: bool = False


@dataclass(slots=True)
class Property:
    name: str
    value: str
    source: str
    reference_label: str | None = None
    reference_url: str | None = None
    preferred: bool = False


@dataclass(frozen=True, slots=True)
class Identifier:
    catalogue: str
    value: str
    source: str


@dataclass(slots=True)
class ObjectRecord:
    kind: str
    name: str
    source: str
    identifiers: list[Identifier] = field(default_factory=list)
    measurements: list[Measurement] = field(default_factory=list)
    properties: list[Property] = field(default_factory=list)

    def preferred(self, quantity: str) -> Measurement | None:
        for measurement in self.measurements:
            if measurement.quantity == quantity and measurement.preferred:
                return measurement
        return None

    def preferred_value(self, quantity: str) -> float | None:
        measurement = self.preferred(quantity)
        return None if measurement is None else measurement.value
