#!/usr/bin/python

import argparse
import subprocess
import time
import os
import sys

# PostgreSQL settings
POSTGRES_USER = "postgres"
POSTGRES_PASSWORD = "password"
POSTGRES_DB = "testdb"
POSTGRES_PORT = "15432"

pq_env = os.environ.copy()
pq_env["PGPASSWORD"] = POSTGRES_PASSWORD

class PostgresqlWrapper:
    """Wrapper for managing PostgreSQL for unit tests using docker-compose."""

    def start_postgres(self):
        """Starts a PostgreSQL instance using docker-compose."""
        compose_file_path = os.path.join(os.path.dirname(__file__), "docker-compose.yml")
        subprocess.run(["docker-compose", "-f", compose_file_path, "up", "-d"], check=True)

        # Wait for PostgreSQL to start
        time.sleep(10)

    def stop_postgres(self):
        """Stops the PostgreSQL instance using docker-compose."""
        compose_file_path = os.path.join(os.path.dirname(__file__), "docker-compose.yml")
        subprocess.run(["docker-compose", "-f", compose_file_path, "down"], check=True)

    def drop_database(self):
        """Drops the test database."""
        try:
            subprocess.run(
                ["psql", "-h", "localhost", "-p", POSTGRES_PORT, "-U", POSTGRES_USER, "-w", "-c", f"DROP DATABASE IF EXISTS {POSTGRES_DB}"],
                capture_output=True,
                check=True,
                env=pq_env
            )
        except subprocess.CalledProcessError as e:
            print(e.stdout.decode())
            print(e.stderr.decode())

            # Ignore error if database doesn't exist
            if "does not exist" not in e.stderr.decode():
                raise

def main():
    """Runs the C++ unit tests with PostgreSQL."""

    parser = argparse.ArgumentParser(description="Run C++ unit tests with PostgreSQL.")
    parser.add_argument("--ci", action="store_true", help="Enable CI mode (does not manage postgresql service)")
    parser.add_argument("unit_tests", nargs="+", help="Paths to the unit test binaries")
    args = parser.parse_args()

    postgres_wrapper = PostgresqlWrapper()

    UNIT_TESTS = args.unit_tests
    if len(UNIT_TESTS) == 0:
        print("Please provide unit tests")
        exit(1)

    try:
        if not args.ci:
            # Start PostgreSQL
            print("Starting PostgreSQL...")
            postgres_wrapper.start_postgres()

        # Run C++ unit tests (actual binaries)
        print("Running C++ unit tests...")
        for unit_test in UNIT_TESTS:
            print(f"Running {unit_test}..")
            result = subprocess.run([unit_test, "--force-colors=true"], capture_output=True, check=False)

            # Print test output
            print(result.stdout.decode())
            print(result.stderr.decode())

            if result.returncode != 0:
                exit(1)  # Exit with error code if tests fail

    finally:
        # Drop test database
        print("Dropping test database...")
        postgres_wrapper.drop_database()

        # Stop PostgreSQL
        if not args.ci:
            print("Stopping PostgreSQL...")
            postgres_wrapper.stop_postgres()

if __name__ == "__main__":
    main()
