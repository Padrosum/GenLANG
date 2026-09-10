package genlang;

public final class Ref {
    public final String name;

    public Ref(String name) {
        this.name = name == null ? "" : name;
    }

    @Override
    public String toString() {
        return "@" + name;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof Ref && name.equals(((Ref) other).name);
    }

    @Override
    public int hashCode() {
        return name.hashCode();
    }
}
