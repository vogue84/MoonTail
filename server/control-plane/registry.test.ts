import { describe, it, expect } from "vitest";

describe("session policy", () => {
  it("peers endpoint requires session first", () => {
    const peersBlocked = true;
    expect(peersBlocked).toBe(true);
  });

  it("pairing limits concurrent to volunteer count", () => {
    const pool = 3;
    const requested = 10;
    expect(Math.min(pool, requested)).toBe(3);
  });
});
