"use strict";

const fs = require("fs");
const path = require("path");

function loadAddon() {
  const env = process.env.GENLANG_NODE_MODULE;
  const root = path.resolve(__dirname, "..", "..");
  const candidates = [
    env,
    path.join(__dirname, "build", "Release", "genlang.node"),
    path.join(root, "build", "genlang.node"),
  ].filter(Boolean);
  for (const candidate of candidates) {
    const resolved = path.resolve(candidate);
    if (fs.existsSync(resolved)) {
      return require(resolved);
    }
  }
  throw new Error(
    "could not load genlang.node; build libgenlang and the N-API addon (GENLANG_NODE_MODULE)"
  );
}

const native = loadAddon();

class Ref {
  constructor(name) {
    this.name = name;
  }
  toString() {
    return "@" + this.name;
  }
}

function wrapValue(value) {
  if (value == null) {
    return value;
  }
  if (Array.isArray(value)) {
    return value.map(wrapValue);
  }
  if (typeof value === "object" && Object.prototype.hasOwnProperty.call(value, "$ref") &&
      Object.keys(value).length === 1) {
    return new Ref(value.$ref);
  }
  if (typeof value === "object") {
    const out = {};
    for (const [key, item] of Object.entries(value)) {
      out[key] = wrapValue(item);
    }
    return out;
  }
  return value;
}

class Document {
  constructor(handle) {
    this._n = handle;
  }

  close() {
    if (this._n) {
      this._n.close();
      this._n = null;
    }
  }

  _native() {
    if (!this._n) {
      throw new Error("document is closed");
    }
    return this._n;
  }

  get types() {
    return this._native().types();
  }

  get sets() {
    return this._native().sets();
  }

  get entities() {
    return this._native().entities();
  }

  typeParent(name) {
    return this._native().typeParent(name);
  }

  entityType(name) {
    return this._native().entityType(name);
  }

  entity(name) {
    return wrapValue(this._native().entity(name));
  }

  get(pathName) {
    return wrapValue(this._native().get(pathName));
  }

  ancestors(name) {
    return this._native().ancestors(name);
  }

  members(setName) {
    return this._native().members(setName);
  }

  isMember(setName, entity) {
    return this._native().isMember(setName, entity);
  }

  serialize() {
    return this._native().serialize();
  }

  toJson() {
    return this._native().toJson();
  }

  toYaml() {
    return this._native().toYaml();
  }

  toBinary() {
    return Buffer.from(this._native().toBinary());
  }
}

function parse(source) {
  return new Document(native.parse(source));
}

function load(filePath) {
  return new Document(native.load(filePath));
}

function version() {
  return native.version();
}

module.exports = {
  Document,
  Ref,
  parse,
  load,
  version,
};
