def generate_uniform_barycentric_coords(n):
    barycentric_coords = []

    for i in range(n + 1):
        for j in range(n - i + 1):
            lambda1 = 1.0 - (i / n) - (j / n)
            lambda2 = i / n
            lambda3 = j / n
            barycentric_coords.append((lambda1, lambda2, lambda3))

    return barycentric_coords

coords = generate_uniform_barycentric_coords(25)

print(f"std::array<glm::vec3, {len(coords)}> baryVec = {{")
for c in coords:
    print(f"    glm::vec3({c[0]}f, {c[1]}f, {c[2]}f),")
print(f"}};");