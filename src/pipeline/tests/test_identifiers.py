import pytest

from stellar_pipeline import identifiers


@pytest.mark.parametrize(
    ("letter", "constellation", "expected"),
    [
        ("Alp", "CMa", "alf CMa"),
        ("The-1", "Ori", "tet1 Ori"),
        ("Xi", "UMa", "ksi UMa"),
        ("p", "Eri", "p Eri"),
    ],
)
def test_bayer_designations_follow_simbad(letter: str, constellation: str, expected: str) -> None:
    assert identifiers.bayer(letter, constellation) == expected


def test_flamsteed() -> None:
    assert identifiers.flamsteed("61", "Cyg") == "61 Cyg"


@pytest.mark.parametrize(
    ("designation", "expected"),
    [
        ("del Pav", ["Delta Pavonis"]),
        ("alf1 Cen", ["Alpha 1 Centauri", "Alpha Centauri"]),
        ("ksi UMa", ["Xi Ursae Majoris"]),
        ("tet1 Ori", ["Theta 1 Orionis", "Theta Orionis"]),
        ("61 Cyg", ["61 Cygni"]),
        ("p Eri", ["p Eridani"]),
        ("eps Boo", ["Epsilon Bootis"]),
    ],
)
def test_spoken_forms_write_out_letters_and_genitives(
    designation: str, expected: list[str]
) -> None:
    assert identifiers.spoken(designation) == expected


def test_every_constellation_has_a_genitive() -> None:
    assert len(identifiers.GENITIVES) == 88


@pytest.mark.parametrize(
    ("designation", "expected"),
    [("Gl 244A", "244A"), ("GJ 1293", "1293"), ("Gl 4.1", "4.1"), ("Gl   4", "4")],
)
def test_gliese_numbers_drop_the_prefix(designation: str, expected: str) -> None:
    assert identifiers.gliese(designation) == expected


@pytest.mark.parametrize(
    ("designation", "expected"),
    [
        ("HIP 70890", ("HIP", "70890")),
        ("HD 10700", ("HD", "10700")),
        ("HD 133131 A", ("HD", "133131")),
        ("HIP 73674 A", ("HIP", "73674")),
        ("GJ 667 C", ("GJ", "667C")),
        ("Gliese 12", ("GJ", "12")),
        ("Gl 581", ("GJ", "581")),
        ("Kepler-22", None),
        ("TOI-700", None),
    ],
)
def test_parse_prefixed(designation: str, expected: tuple[str, str] | None) -> None:
    assert identifiers.parse_prefixed(designation) == expected


def test_unknown_bayer_letter_is_an_error() -> None:
    with pytest.raises(ValueError, match="Bayer"):
        identifiers.bayer("Alp_1", "Cen")
